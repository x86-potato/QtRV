#pragma once

#include <string>
#include <vector>
#include <array>
#include <functional>
#include <algorithm>
#include <regex>

#include "Lexer.h"
#include "Assembler.h"  
#include "MipsCPU.h"
#include "Buffer.h"
#include "Binary.h"
#include "parser.h"
#include "Memory.h"
#include "Loader.h"
#include "InstructionState.h"

using PCtoLine = std::map<uint32_t, uint32_t>;
using LinetoPC = std::map<uint32_t, uint32_t>;

// One file's worth of source text to be concatenated into the assembled blob.
// `path` is an absolute file path for on-disk files, or a synthetic key
// (e.g. "untitled://...") for the unsaved quick-start buffer.
struct SourceUnit
{
    std::string path;
    std::string text;
};

// Records where one file's lines landed in the concatenated blob, so
// blob-relative line numbers (used everywhere internally) can be translated
// back to a (file, local line) pair for breakpoints and execution highlighting.
struct FileSpan
{
    std::string path;
    uint32_t    startLine = 0; // first blob line (1-indexed) belonging to this file
    uint32_t    lineCount = 0;
};

class Emulator
{
public:
    Emulator() = default;

    template <typename Callback>
    bool loadProgram(const std::string& source, Callback callback)
    {
        reset();
        auto tokens = m_lexer.tokenize(source);
        callback();

        auto IR = m_parser.parse(tokens);
        callback();

        // m_fileTable is normally populated by loadProgramMulti() just before
        // it calls this; a direct/standalone loadProgram() call (no file
        // table set up yet) falls back to treating the whole source as one
        // file, so label scoping degrades to "everything is global", same
        // as before file scoping existed.
        std::vector<AssemblerFileSpan> fileSpans;
        if (!m_fileTable.empty())
        {
            fileSpans.reserve(m_fileTable.size());
            for (const auto& span : m_fileTable)
                fileSpans.push_back({span.startLine, span.lineCount});
        }
        else
        {
            uint32_t lineCount = static_cast<uint32_t>(std::count(source.begin(), source.end(), '\n')) + 1;
            fileSpans.push_back({1u, lineCount});
        }

        auto binary = m_assembler.assemble(IR, fileSpans);
        m_PCtoLineMap = std::move(m_assembler.PCtoLine);
        m_lineToPCMap = std::move(m_assembler.LinetoPC);

        Loader::load(binary, m_memory, m_cpu.text, m_cpu.data);
        m_cpu.pc      = m_cpu.text;
        m_cpu.textEnd = m_cpu.text + static_cast<uint32_t>(binary.bin.size());
        m_cpu.setMemory(&m_memory);
        m_cpu.setOutput(&m_buffer);
        m_cpu.setInputCallback(m_inputCallback);
        m_cpu.setSleepCallback(m_sleepCallback);

        InitBreakpoints();

        return true;
    }

    // Concatenates every unit's source text into one blob ("header style" —
    // no real linking) and records a FileSpan per unit so blob line numbers
    // can be translated back to (file, local line) for breakpoints and the
    // execution-highlight cursor. Single-file callers just pass one unit.
    template <typename Callback>
    bool loadProgramMulti(const std::vector<SourceUnit>& units, Callback callback)
    {
        std::string blob;
        m_fileTable.clear();

        uint32_t lineCursor = 0; // lines emitted so far (0-indexed count == next start line - 1)
        for (const auto& unit : units)
        {
            std::string text = unit.text;
            if (!text.empty() && text.back() != '\n')
                text += '\n';

            uint32_t lineCount = static_cast<uint32_t>(std::count(text.begin(), text.end(), '\n'));

            FileSpan span;
            span.path      = unit.path;
            span.startLine = lineCursor + 1; // 1-indexed
            span.lineCount = lineCount;
            m_fileTable.push_back(span);

            blob += text;
            lineCursor += lineCount;
        }

        m_sourceBlob = blob;
        return loadProgram(blob, callback);
    }

    // Translates a blob-relative (1-indexed) line number to the file path it
    // belongs to. Returns an empty string if it falls outside every span
    // (e.g. the trailing EOF line).
    std::string fileForBlobLine(uint32_t blobLine) const
    {
        for (const auto& span : m_fileTable) {
            if (blobLine >= span.startLine && blobLine < span.startLine + span.lineCount)
                return span.path;
        }
        return "";
    }

    // Translates a blob-relative (1-indexed) line number to a 1-indexed line
    // local to whichever file owns it.
    uint32_t localLineForBlobLine(uint32_t blobLine) const
    {
        for (const auto& span : m_fileTable) {
            if (blobLine >= span.startLine && blobLine < span.startLine + span.lineCount)
                return blobLine - span.startLine + 1;
        }
        return 0;
    }

    // Inverse of the above: given a file path and a 1-indexed local line,
    // returns the corresponding blob-relative line, or 0 if that file isn't
    // part of the currently loaded program.
    uint32_t blobLineForFileLocal(const std::string& path, uint32_t localLine) const
    {
        for (const auto& span : m_fileTable) {
            if (span.path == path)
                return span.startLine + localLine - 1;
        }
        return 0;
    }

    // Lexer/Parser/Assembler exceptions report blob-relative line numbers
    // ("... at line 42"), which are meaningless once multiple files are
    // compiled together. This rewrites every "line N" in a message to
    // "line <local line> of <file>" so errors read naturally regardless of
    // how many files were involved.
    std::string annotateFileLines(const std::string &message) const
    {
        static const std::regex lineRef(R"(line (\d+))");

        std::string result;
        result.reserve(message.size());
        size_t lastEnd = 0;

        for (auto it = std::sregex_iterator(message.begin(), message.end(), lineRef);
             it != std::sregex_iterator(); ++it)
        {
            const std::smatch &m = *it;
            size_t matchStart = static_cast<size_t>(m.position(0));
            result.append(message, lastEnd, matchStart - lastEnd);

            uint32_t blobLine = static_cast<uint32_t>(std::stoul(m[1].str()));
            std::string filePath = fileForBlobLine(blobLine);
            uint32_t localLine = localLineForBlobLine(blobLine);

            if (filePath.empty() || localLine == 0)
            {
                result.append(m.str(0)); // couldn't resolve; leave the original text as-is
            }
            else
            {
                bool isUnsaved = filePath.rfind("untitled://", 0) == 0;
                std::string label = isUnsaved ? "Untitled" : filePath.substr(filePath.find_last_of("/\\") + 1);
                result += "line " + std::to_string(localLine) + " of " + label;
            }

            lastEnd = matchStart + static_cast<size_t>(m.length(0));
        }
        result.append(message, lastEnd, std::string::npos);
        return result;
    }

    // Translates a MipsRuntimeError's faulting PC to a blob-relative "at line
    // N" suffix (via m_PCtoLineMap) so it flows through the same
    // annotateFileLines() rewriting that assembler/parser errors already use,
    // giving runtime traps (overflow, bad address) file+line just like them.
    std::string annotateRuntimeError(const MipsRuntimeError& e) const
    {
        std::string msg = e.what();
        auto it = m_PCtoLineMap.find(e.pc);
        if (it != m_PCtoLineMap.end())
            msg += " at line " + std::to_string(it->second);
        return msg;
    }

    template <typename Callback>
    void run(Callback callback, uint32_t maxCycles = UINT32_MAX)
    {
        // Don't run if the program ended naturally
        if (m_cpu.halted() && !m_cpu.m_breakpointHit) return;

        // If we are parked on a breakpoint, configure CPU to step past it
        if (m_cpu.m_breakpointHit) {
            m_cpu.m_halted = false;
            m_cpu.m_breakpointHit = false;
            m_cpu.m_ignoreNextBreakpoint = true;
        }

        uint32_t cycles = 0;
        while (!m_cpu.halted()) {
            try {
                if(m_pipelineMode) {
                    m_cpu.cycleTick();
                } else {
                    m_cpu.tick();
                }
            } catch (const MipsRuntimeError& e) {
                m_cpu.m_halted = true;
                throw std::runtime_error(annotateRuntimeError(e));
            }

            //construct the future instruction state for the pipeline panel
            updatePipelineStates();
            
            // If the tick naturally hit a breakpoint, break the run loop
            if (m_cpu.m_breakpointHit) {
                callback();
                break;
            }
            
            ++cycles;
            callback();
        }
    }

// --- PIPELINE TRACKING VARIABLES ---
    int m_globalCycle = 0;
    std::vector<PipelineRow> m_pipelineHistory;
    // Tracks the active instruction index in m_pipelineHistory for: [WB, MEM, EX, ID, IF]
    int m_activeInsts[5] = {-1, -1, -1, -1, -1}; 

    void updatePipelineStates()
    {
        if (!m_pipelineMode) return;
        m_globalCycle++;

        // 1. Log stages completed in the current cycle
        if (m_activeInsts[0] != -1) m_pipelineHistory[m_activeInsts[0]].stages.push_back("WB");
        if (m_activeInsts[1] != -1) m_pipelineHistory[m_activeInsts[1]].stages.push_back("MEM");
        if (m_activeInsts[2] != -1) m_pipelineHistory[m_activeInsts[2]].stages.push_back("EX");
        
        if (m_activeInsts[3] != -1) {
            if (m_cpu.m_stall) m_pipelineHistory[m_activeInsts[3]].stages.push_back("STALL");
            else m_pipelineHistory[m_activeInsts[3]].stages.push_back("ID");
        }
        
        if (m_activeInsts[4] != -1) {
            if (m_cpu.m_stall) m_pipelineHistory[m_activeInsts[4]].stages.push_back("STALL");
            else m_pipelineHistory[m_activeInsts[4]].stages.push_back("IF");
        }

        // 2. Shift the tracker for the next cycle
        m_activeInsts[0] = m_cpu.MEM_WB.valid ? m_activeInsts[1] : -1;
        m_activeInsts[1] = m_cpu.EX_MEM.valid ? m_activeInsts[2] : -1;
        
        if (m_cpu.m_stall) {
            m_activeInsts[2] = -1; // EX gets a bubble
            // ID and IF stay in place
        } else {
            m_activeInsts[2] = m_cpu.ID_EX.valid ? m_activeInsts[3] : -1;
            
            // Check if IF was flushed (invalidated) by a branch in EX
            if (!m_cpu.IF_ID.valid && m_activeInsts[4] != -1) {
                m_pipelineHistory[m_activeInsts[4]].stages.push_back("FLUSH");
                m_activeInsts[3] = -1; // Disappears from the pipeline
            } else {
                m_activeInsts[3] = m_activeInsts[4]; // Moves to ID normally
            }
            
            // 3. Bring a newly fetched instruction into the IF tracker
            if (m_cpu.IF_ID.valid) {
                PipelineRow newRow;
                newRow.pc = m_cpu.IF_ID.pc;
                newRow.lineStr = getInstructionAtPC(m_cpu.IF_ID.pc);
                newRow.startCycle = m_globalCycle; 
                m_pipelineHistory.push_back(newRow);
                
                m_activeInsts[4] = m_pipelineHistory.size() - 1;
            } else {
                m_activeInsts[4] = -1;
            }
        }
    }

    void halt();
    void reset(); 
    void debugTokens(const std::vector<Token>& tokens);
    void debugIR(const Program& program);

    std::string getInstructionAtPC(uint32_t pc) const
    {
        auto it = m_PCtoLineMap.find(pc);
        if (it == m_PCtoLineMap.end()) return "";

        uint32_t line = it->second;
        auto it2 = m_lineToPCMap.find(line);
        if (it2 == m_lineToPCMap.end()) return "";

        uint32_t pcAtLine = it2->second;
        if (pcAtLine != pc) return "";

        // Now we can find the instruction in the assembler's IR
        for (const auto& [irPC, irLine] : m_PCtoLineMap) {
            if (irPC == pc && irLine == line) {
                // We found the matching instruction
                // For simplicity, let's just return a placeholder string
                return std::to_string(irLine);
            }
        }

        return "";
    }


    void setInputCallback(std::function<std::string(const std::string&)> cb)
    {
        m_inputCallback = std::move(cb);
    }

    void setSleepCallback(std::function<void(uint32_t)> cb)
    {
        m_sleepCallback = std::move(cb);
    }

    void step() { 
        if (m_cpu.halted() && !m_cpu.m_breakpointHit) return; // Ignore if program finished naturally

        // If stepping from a breakpoint, step past it
        if (m_cpu.m_breakpointHit) {
            m_cpu.m_breakpointHit = false;
            m_cpu.m_halted = false;
            m_cpu.m_ignoreNextBreakpoint = true;
        } 
        
        try {
            if(m_pipelineMode) {
                m_cpu.cycleTick();
            } else {
                m_cpu.tick();
            }
        } catch (const MipsRuntimeError& e) {
            m_cpu.m_halted = true;
            throw std::runtime_error(annotateRuntimeError(e));
        }

        updatePipelineStates();
    }

    // fileKey: absolute path (or synthetic key for an unsaved buffer);
    // localLine: 0-indexed, matching CodeEditor's block numbers.
    void setBreakpoint(const std::string& fileKey, uint32_t localLine, bool enabled)
    {
        if (enabled) {
            m_breakpointsByFile[fileKey].insert(localLine);
        } else {
            auto it = m_breakpointsByFile.find(fileKey);
            if (it != m_breakpointsByFile.end())
                it->second.erase(localLine);
        }

        // If this file is part of the currently loaded program, mirror the
        // change onto the CPU immediately (matches the old behavior).
        uint32_t blobLine = blobLineForFileLocal(fileKey, localLine + 1);
        if (blobLine != 0 && m_lineToPCMap.find(blobLine) != m_lineToPCMap.end())
            m_cpu.setBreakpoint(m_lineToPCMap[blobLine], enabled);
    }

    // Moves a file's breakpoint bucket to a new key, e.g. when an unsaved
    // buffer is saved to disk for the first time and its key changes from a
    // synthetic id to a real absolute path.
    void renameBreakpointFile(const std::string& oldKey, const std::string& newKey)
    {
        auto it = m_breakpointsByFile.find(oldKey);
        if (it == m_breakpointsByFile.end()) return;
        m_breakpointsByFile[newKey] = std::move(it->second);
        m_breakpointsByFile.erase(it);
    }

    void InitBreakpoints()
    {
        for (const auto& span : m_fileTable)
        {
            auto it = m_breakpointsByFile.find(span.path);
            if (it == m_breakpointsByFile.end()) continue;

            for (uint32_t localLine : it->second)
            {
                uint32_t blobLine = span.startLine + localLine; // localLine is 0-indexed
                if (m_lineToPCMap.find(blobLine) != m_lineToPCMap.end())
                    m_cpu.setBreakpoint(m_lineToPCMap[blobLine], true);
            }
        }
    }

    bool halted() const { return m_cpu.halted(); }
    bool isFinished() const { return m_cpu.halted() && !m_cpu.m_breakpointHit; }
    bool isBreakpoint() const { return m_cpu.m_breakpointHit; }

    std::array<uint32_t, 32> registers() const
    {
        std::array<uint32_t, 32> arr;
        std::copy(std::begin(m_cpu.r), std::end(m_cpu.r), arr.begin());
        return arr;
    }

    uint32_t pc() const { return m_cpu.pc; }

    const std::vector<std::string>& Errors() const { return m_errors; }

    Memory& memory()       { return m_memory; }
    uint32_t textBase() const { return m_cpu.text; }

    Buffer m_buffer;
    PCtoLine    m_PCtoLineMap;
    LinetoPC    m_lineToPCMap;
    std::map<std::string, std::unordered_set<uint32_t>> m_breakpointsByFile;
    std::vector<PipelineRow> m_pipelineStates; // Store the pipeline states for visualization
    bool m_pipelineMode = false;

    std::string          m_sourceBlob; // concatenated source text for the currently loaded program
    std::vector<FileSpan> m_fileTable; // per-file line spans within m_sourceBlob

private:
    MipsCPU     m_cpu;
    Memory      m_memory;
    Lexer       m_lexer;
    Parser      m_parser;
    Assembler   m_assembler;

    std::function<std::string(const std::string&)> m_inputCallback;
    std::function<void(uint32_t)> m_sleepCallback;
    std::vector<std::string> m_errors;

};
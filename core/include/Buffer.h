#pragma once

#include <vector>
#include <string>

struct Buffer
{
    std::string m_data;

    void clear()
    {
        m_data.clear();
    }
};
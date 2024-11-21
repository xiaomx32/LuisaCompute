//
// Created by swfly on 2024/11/21.
//

#pragma once
#include <vector>

class FallbackBuffer
{
public:

    void* addr(){return data.data();};


private:

    std::vector<std::byte> data{};

};

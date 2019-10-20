// Copyright (c) 2019 Nicholas Corgan
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <arrayfire.h>

#include <string>
#include <vector>

struct DeviceCacheEntry
{
    std::string name;
    std::string platform;
    std::string toolkit;
    std::string compute;
    size_t memoryStepSize;

    ::af_backend afBackendEnum;
    int afDeviceIndex;
};

const std::vector<DeviceCacheEntry>& getDeviceCache();

/*
#include <Pothos/Framework.hpp>

class ArrayFireBlock: public Pothos::Block
{
    public:
        ArrayFireBlock();
        virtual ~ArrayFireBlock();

        af::array getInputPortAsAfArray(
            size_t portNum,
            bool truncateToMinLength = true);

        af::array getInputPortAsAfArray(
            const std::string& portName,
            bool truncateToMinLength = true);

        void postAfArray(
            size_t portNum,
            const af::array& afArray);

        void postAfArray(
            const std::string& portName,
            const af::array& afArray);

    private:

        template <typename T>
        af::array _getInputPortAsAfArray(
            const T& portId,
            bool truncateToMinLength);

        template <typename T>
        void _postAfArray(
            const T& portId,
            const af::array& afArray);
};
*/

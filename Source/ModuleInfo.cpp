// Copyright (c) 2019 Nicholas Corgan
// SPDX-License-Identifier: BSD-3-Clause

#include "DeviceCache.hpp"
#include "Utility.hpp"

#include <Pothos/Plugin.hpp>

#include <json.hpp>

#include <arrayfire.h>

#include <Poco/String.h>

#include <algorithm>
#include <iostream>
#include <set>

using json = nlohmann::json;

static json deviceCacheEntryToJSON(const DeviceCacheEntry& entry)
{
    json deviceJSON;
    deviceJSON["Name"] = entry.name;
    deviceJSON["Platform"] = entry.platform;
    deviceJSON["Toolkit"] = entry.toolkit;
    deviceJSON["Compute"] = entry.compute;
    deviceJSON["Memory Step Size"] = entry.memoryStepSize;

    return deviceJSON;
}

static std::string _enumerateArrayFireDevices()
{
    json topObject;
    auto& arrayFireInfo = topObject["PothosGPU Library Info"];
    arrayFireInfo["ArrayFire Version"] = AF_VERSION;

    const auto& deviceCache = getDeviceCache();
    const auto& afAvailableBackends = getAvailableBackends();

    json devicesJSON(json::array());
    std::transform(
        std::begin(deviceCache),
        std::end(deviceCache),
        std::back_inserter(devicesJSON),
        deviceCacheEntryToJSON);
    topObject["PothosGPU Device"] = devicesJSON;

    std::vector<std::string> availableBackends;
    std::transform(
        std::begin(afAvailableBackends),
        std::end(afAvailableBackends),
        std::back_inserter(availableBackends),
        [](af::Backend backend)
        {return Pothos::Object(backend).convert<std::string>();});

    arrayFireInfo["Available Backends"] = Poco::cat(
                                              std::string(", "),
                                              std::begin(availableBackends),
                                              std::end(availableBackends));

    return topObject.dump();
}

static std::string enumerateArrayFireDevices()
{
    // Only do this once
    static const std::string devs = _enumerateArrayFireDevices();

    return devs;
}

pothos_static_block(registerGPUInfo)
{
    Pothos::PluginRegistry::addCall(
        "/devices/gpu/info", &enumerateArrayFireDevices);
}

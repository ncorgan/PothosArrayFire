// Copyright (c) 2019-2020 Nicholas Corgan
// SPDX-License-Identifier: BSD-3-Clause

#include "TestUtility.hpp"

#include <Pothos/Framework.hpp>
#include <Pothos/Proxy.hpp>
#include <Pothos/Testing.hpp>

#include <Poco/TemporaryFile.h>
#include <Poco/Timestamp.h>

#include <arrayfire.h>

#include <iostream>
#include <string>
#include <vector>

namespace GPUTests
{

static const std::vector<std::string> AllTypes =
{
    "int8",
    "int16",
    "int32",
    "int64",
    "uint8",
    "uint16",
    "uint32",
    "uint64",
    "float32",
    "float64",
    "complex_float32",
    "complex_float64"
};
struct TestData
{
    Pothos::DType dtype;

    std::string oneDimKey;
    std::string twoDimKey;

    Pothos::BufferChunk oneDimArray;
    std::vector<Pothos::BufferChunk> twoDimArray;
};

static void testFileSink1D(
    const std::string& filepath,
    const TestData& testData)
{
    std::cout << "Testing " << testData.dtype.name()
              << " (chans: 1)..." << std::endl;

    auto oneDimBlock = Pothos::BlockRegistry::make(
                           "/gpu/array/file_sink",
                           filepath,
                           testData.oneDimKey,
                           testData.dtype,
                           1 /*numChannels*/,
                           false /*append*/);
    auto feederSource = Pothos::BlockRegistry::make(
                            "/blocks/feeder_source",
                            testData.dtype);
    feederSource.call("feedBuffer", testData.oneDimArray);

    POTHOS_TEST_EQUAL(
        filepath,
        oneDimBlock.call<std::string>("filepath"));
    POTHOS_TEST_EQUAL(
        testData.oneDimKey,
        oneDimBlock.call<std::string>("key"));
    POTHOS_TEST_FALSE(oneDimBlock.call<bool>("append"));
    POTHOS_TEST_EQUAL(0, oneDimBlock.call("outputs").call<size_t>("size"));

    const auto inputs = oneDimBlock.call<InputPortVector>("inputs");
    POTHOS_TEST_EQUAL(1, inputs.size());
    POTHOS_TEST_EQUAL(
        testData.dtype.name(),
        inputs[0]->dtype().name());

    // Execute the topology.
    {
        Pothos::Topology topology;
        topology.connect(
            feederSource,
            0,
            oneDimBlock,
            0);

        topology.commit();
        POTHOS_TEST_TRUE(topology.waitInactive(0.05));
    }

    // Read back and compare array.
    POTHOS_TEST_TRUE(Poco::File(filepath).exists());
    POTHOS_TEST_NOT_EQUAL(-1, af::readArrayCheck(filepath.c_str(), testData.oneDimKey.c_str()));

    auto arrFromFile = af::readArray(filepath.c_str(), testData.oneDimKey.c_str());
    POTHOS_TEST_EQUAL(1, arrFromFile.numdims());
    compareAfArrayToBufferChunk(arrFromFile, testData.oneDimArray);
}

static void testFileSink2D(
    const std::string& filepath,
    const TestData& testData)
{
    const auto nchans = testData.twoDimArray.size();

    std::cout << "Testing " << testData.dtype.name()
              << " (chans: " << nchans << ")..." << std::endl;

    auto twoDimBlock = Pothos::BlockRegistry::make(
                           "/gpu/array/file_sink",
                           filepath,
                           testData.twoDimKey,
                           testData.dtype,
                           nchans /*numChannels*/,
                           false /*append*/);

    std::vector<Pothos::Proxy> feederSources;
    for(size_t chan = 0; chan < nchans; ++chan)
    {
        feederSources.emplace_back(Pothos::BlockRegistry::make(
                                       "/blocks/feeder_source",
                                       testData.dtype));
        feederSources.back().call(
            "feedBuffer",
            testData.twoDimArray[chan]);
    }

    POTHOS_TEST_EQUAL(
        filepath,
        twoDimBlock.call<std::string>("filepath"));
    POTHOS_TEST_EQUAL(
        testData.twoDimKey,
        twoDimBlock.call<std::string>("key"));
    POTHOS_TEST_FALSE(twoDimBlock.call<bool>("append"));
    POTHOS_TEST_EQUAL(0, twoDimBlock.call("outputs").call<size_t>("size"));

    const auto inputs = twoDimBlock.call<InputPortVector>("inputs");
    POTHOS_TEST_EQUAL(nchans, inputs.size());
    for(auto* input: inputs)
    {
        POTHOS_TEST_EQUAL(
            testData.dtype.name(),
            input->dtype().name());
    }

    // Execute the topology.
    {
        Pothos::Topology topology;
        for(size_t chan = 0; chan < nchans; ++chan)
        {
            topology.connect(
                feederSources[chan],
                0,
                twoDimBlock,
                chan);
        }

        topology.commit();
        POTHOS_TEST_TRUE(topology.waitInactive(0.05));
    }

    // Read back and compare array.
    POTHOS_TEST_TRUE(Poco::File(filepath).exists());
    POTHOS_TEST_NOT_EQUAL(-1, af::readArrayCheck(filepath.c_str(), testData.twoDimKey.c_str()));

    auto arrFromFile = af::readArray(filepath.c_str(), testData.twoDimKey.c_str());
    POTHOS_TEST_EQUAL(2, arrFromFile.numdims());
    POTHOS_TEST_EQUAL(testData.twoDimArray.size(), size_t(arrFromFile.dims(0)));
    POTHOS_TEST_EQUAL(testData.twoDimArray[0].elements(), size_t(arrFromFile.dims(1)));

    for(size_t chan = 0; chan < nchans; ++chan)
    {
        compareAfArrayToBufferChunk(arrFromFile.row(chan), testData.twoDimArray[chan]);
    }
}

}

POTHOS_TEST_BLOCK("/gpu/tests", test_file_sink)
{
    using namespace GPUTests;

    setupTestEnv();

    af::setSeed(static_cast<unsigned long long>(Poco::Timestamp().utcTime()));

    constexpr dim_t numChannels = 4;
    constexpr dim_t numElements = 50;

    std::vector<TestData> allTestData;
    for(const std::string& type: AllTypes)
    {
        auto afDType = Pothos::Object(type).convert<af::dtype>();

        auto oneDimArray = af::randu(numElements, afDType);
        auto twoDimArray = af::randu(numChannels, numElements, afDType);

        GPUTests::addMinMaxToAfArray(oneDimArray);
        GPUTests::addMinMaxToAfArray(twoDimArray);
        TestData testData{
            Pothos::DType(type),
            ("1d_" + type),
            ("2d_" + type),
            Pothos::Object(oneDimArray).convert<Pothos::BufferChunk>(),
            GPUTests::convert2DAfArrayToBufferChunks(twoDimArray)
        };

        allTestData.emplace_back(std::move(testData));
    }

    Poco::TemporaryFile tempFile;
    tempFile.keepUntilExit();

    const auto& testDataFilepath = tempFile.path();

    for(const auto& testData: allTestData)
    {
        testFileSink1D(
            testDataFilepath,
            testData);
        testFileSink2D(
            testDataFilepath,
            testData);
    }

    // TODO: test trying to append with wrong type, wrong dims
    // TODO: test valid appending
}

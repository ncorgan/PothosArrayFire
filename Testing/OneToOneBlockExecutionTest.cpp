// Copyright (c) 2019 Nicholas Corgan
// SPDX-License-Identifier: BSD-3-Clause

#include "BlockExecutionTests.hpp"
#include "TestUtility.hpp"

#include <Pothos/Framework.hpp>
#include <Pothos/Proxy.hpp>
#include <Pothos/Testing.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <typeinfo>

template <typename In, typename Out>
void testOneToOneBlockCommon(
    const Pothos::Proxy& block,
    const UnaryFunc<In, Out>& verificationFunc)
{
    static const Pothos::DType inputDType(typeid(In));
    static const Pothos::DType outputDType(typeid(Out));

    const size_t numChannels = block.call<PortInfoVector>("inputPortInfo").size();

    std::vector<std::vector<In>> testInputs(numChannels);
    std::vector<Pothos::Proxy> feederSources;
    std::vector<Pothos::Proxy> collectorSinks;

    for(size_t chan = 0; chan < numChannels; ++chan)
    {
        testInputs[chan] = getTestInputs<In>();

        feederSources.emplace_back(
            Pothos::BlockRegistry::make(
                "/blocks/feeder_source",
                inputDType));
        feederSources.back().call(
            "feedBuffer",
            stdVectorToBufferChunk<In>(
                inputDType,
                testInputs[chan]));

        collectorSinks.emplace_back(
            Pothos::BlockRegistry::make(
                "/blocks/collector_sink",
                outputDType));
    }

    // Execute the topology.
    {
        Pothos::Topology topology;
        for(size_t chan = 0; chan < numChannels; ++chan)
        {
            topology.connect(
                feederSources[chan],
                0,
                block,
                chan);
            topology.connect(
                block,
                chan,
                collectorSinks[chan],
                0);
        }

        topology.commit();
        POTHOS_TEST_TRUE(topology.waitInactive(0.05));
    }

    // Make sure the blocks output data and, if the caller provided a
    // verification function, that the outputs are valid.
    for(size_t chan = 0; chan < numChannels; ++chan)
    {
        const auto& chanInputs = testInputs[chan];
        const size_t numInputs = chanInputs.size();

        auto chanOutputs = collectorSinks[chan].call<Pothos::BufferChunk>("getBuffer");
        POTHOS_TEST_EQUAL(
            numInputs,
            chanOutputs.elements());
        if(nullptr != verificationFunc)
        {
            std::vector<Out> expectedOutputs;
            std::transform(
                chanInputs.begin(),
                chanInputs.end(),
                std::back_inserter(expectedOutputs),
                verificationFunc);

            testBufferChunk<Out>(
                chanOutputs,
                expectedOutputs);
        }
    }
}

template <typename T>
void testOneToOneBlock(
    const std::string& blockRegistryPath,
    size_t numChannels,
    const UnaryFunc<T, T>& verificationFunc)
{
    static const Pothos::DType dtype(typeid(T));

    std::cout << "Testing " << blockRegistryPath << " (type: " << dtype.name()
                            << ", " << "chans: " << numChannels << ")" << std::endl;

    auto block = Pothos::BlockRegistry::make(
                     blockRegistryPath,
                     dtype,
                     numChannels);
    auto inputPortInfo = block.call<PortInfoVector>("inputPortInfo");
    auto outputPortInfo = block.call<PortInfoVector>("outputPortInfo");
    POTHOS_TEST_EQUAL(numChannels, inputPortInfo.size());
    POTHOS_TEST_EQUAL(numChannels, outputPortInfo.size());

    testOneToOneBlockCommon<T, T>(
        block,
        verificationFunc);
}

template <typename In, typename Out>
void testOneToOneBlock(
    const std::string& blockRegistryPath,
    size_t numChannels,
    const UnaryFunc<In, Out>& verificationFunc)
{
    static const Pothos::DType inputDType(typeid(In));
    static const Pothos::DType outputDType(typeid(Out));

    std::cout << "Testing " << blockRegistryPath
                            << " (types: " << inputDType.name() << " -> " << outputDType.name()
                            << ", " << "chans: " << numChannels << ")" << std::endl;

    auto block = Pothos::BlockRegistry::make(
                     blockRegistryPath,
                     inputDType,
                     outputDType,
                     numChannels);
    auto inputPortInfo = block.call<PortInfoVector>("inputPortInfo");
    auto outputPortInfo = block.call<PortInfoVector>("outputPortInfo");
    POTHOS_TEST_EQUAL(numChannels, inputPortInfo.size());
    POTHOS_TEST_EQUAL(numChannels, outputPortInfo.size());

    testOneToOneBlockCommon<In, Out>(
        block,
        verificationFunc);
}

#define SPECIALIZE_TEMPLATE_TEST(T) \
    template \
    void testOneToOneBlock<T>( \
        const std::string& blockRegistryPath, \
        size_t numChannels, \
        const UnaryFunc<T, T>& verificationFunc);

#define SPECIALIZE_COMPLEX_TEMPLATE_TEST(T) \
    template \
    void testOneToOneBlock<T, std::complex<T>>( \
        const std::string& blockRegistryPath, \
        size_t numChannels, \
        const UnaryFunc<T, std::complex<T>>& verificationFunc); \
    template \
    void testOneToOneBlock<std::complex<T>, T>( \
        const std::string& blockRegistryPath, \
        size_t numChannels, \
        const UnaryFunc<std::complex<T>, T>& verificationFunc);

SPECIALIZE_TEMPLATE_TEST(std::int8_t)
SPECIALIZE_TEMPLATE_TEST(std::int16_t)
SPECIALIZE_TEMPLATE_TEST(std::int32_t)
SPECIALIZE_TEMPLATE_TEST(std::int64_t)
SPECIALIZE_TEMPLATE_TEST(std::uint8_t)
SPECIALIZE_TEMPLATE_TEST(std::uint16_t)
SPECIALIZE_TEMPLATE_TEST(std::uint32_t)
SPECIALIZE_TEMPLATE_TEST(std::uint64_t)
SPECIALIZE_TEMPLATE_TEST(float)
SPECIALIZE_TEMPLATE_TEST(double)
SPECIALIZE_TEMPLATE_TEST(std::complex<float>)
SPECIALIZE_TEMPLATE_TEST(std::complex<double>)

SPECIALIZE_COMPLEX_TEMPLATE_TEST(float)
SPECIALIZE_COMPLEX_TEMPLATE_TEST(double)

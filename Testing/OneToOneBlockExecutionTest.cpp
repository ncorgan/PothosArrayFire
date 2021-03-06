// Copyright (c) 2019-2020 Nicholas Corgan
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

namespace GPUTests
{

template <typename In, typename Out>
static void testOneToOneBlockCommon(const Pothos::Proxy& block)
{
    static const Pothos::DType inputDType(typeid(In));
    static const Pothos::DType outputDType(typeid(Out));

    auto testInputs = getTestInputs(inputDType.toString());

    auto feederSource = Pothos::BlockRegistry::make(
                            "/blocks/feeder_source",
                            inputDType);
    feederSource.call(
        "feedBuffer",
        testInputs);

    auto collectorSink = Pothos::BlockRegistry::make(
                             "/blocks/collector_sink",
                             outputDType);

    // Execute the topology.
    {
        Pothos::Topology topology;
        topology.connect(feederSource, 0, block, 0);
        topology.connect(block, 0, collectorSink, 0);

        topology.commit();
        POTHOS_TEST_TRUE(topology.waitInactive(0.05));
    }

    // Make sure the block outputs data.
    auto output = collectorSink.call<Pothos::BufferChunk>("getBuffer");
    POTHOS_TEST_EQUAL(
        testInputs.elements(),
        output.elements());
}

template <typename T>
void testOneToOneBlock(const std::string& blockRegistryPath)
{
    static const Pothos::DType dtype(typeid(T));

    std::cout << "Testing " << blockRegistryPath << " (type: " << dtype.name()
                            << ")" << std::endl;

    auto block = Pothos::BlockRegistry::make(
                     blockRegistryPath,
                     "Auto",
                     dtype);
    testOneToOneBlockCommon<T, T>(block);
}

template <typename T>
void testOneToOneBlockF2C(const std::string& blockRegistryPath)
{
    static const Pothos::DType floatDType(typeid(T));
    static const Pothos::DType complexDType(typeid(std::complex<T>));

    std::cout << "Testing " << blockRegistryPath
                            << " (types: " << floatDType.name() << " -> " << complexDType.name()
                            << ")" << std::endl;

    auto block = Pothos::BlockRegistry::make(
                     blockRegistryPath,
                     "Auto",
                     floatDType);
    testOneToOneBlockCommon<T, std::complex<T>>(block);
}

template <typename T>
void testOneToOneBlockC2F(const std::string& blockRegistryPath)
{
    static const Pothos::DType floatDType(typeid(T));
    static const Pothos::DType complexDType(typeid(std::complex<T>));

    std::cout << "Testing " << blockRegistryPath
                            << " (types: " << complexDType.name() << " -> " << floatDType.name()
                            << ")" << std::endl;

    auto block = Pothos::BlockRegistry::make(
                     blockRegistryPath,
                     "Auto",
                     floatDType);

    testOneToOneBlockCommon<std::complex<T>, T>(block);
}

template <typename T>
void testScalarOpBlock(
    const std::string& blockRegistryPath,
    bool allowZeroScalar)
{
    static const Pothos::DType dtype(typeid(T));
    static const T zero(0);

    std::cout << "Testing " << blockRegistryPath << " (type: " << dtype.name()
                            << ")" << std::endl;

    T scalar;
    do
    {
        scalar = getSingleTestInput(dtype.name()).convert<T>();
    } while(!allowZeroScalar && (scalar == zero));

    auto block = Pothos::BlockRegistry::make(
                     blockRegistryPath,
                     "Auto",
                     dtype,
                     scalar);
    testEqual(scalar, block.template call<T>("getScalar"));

    // Test explicit getter+setter.
    block.template call("setScalar", scalar);
    testEqual(scalar, block.template call<T>("getScalar"));

    testOneToOneBlockCommon<T, T>(block);
}

#define SPECIALIZE_TEMPLATE_TEST(T) \
    template \
    void testOneToOneBlock<T>(const std::string&); \
    template \
    void testScalarOpBlock<T>(const std::string&, bool);

#define SPECIALIZE_COMPLEX_1TO1_TEMPLATE_TEST(T) \
    template \
    void testOneToOneBlockF2C<T>(const std::string&); \
    template \
    void testOneToOneBlockC2F<T>(const std::string&);

SPECIALIZE_TEMPLATE_TEST(char)
SPECIALIZE_TEMPLATE_TEST(signed char)
SPECIALIZE_TEMPLATE_TEST(short)
SPECIALIZE_TEMPLATE_TEST(int)
SPECIALIZE_TEMPLATE_TEST(long long)
SPECIALIZE_TEMPLATE_TEST(unsigned char)
SPECIALIZE_TEMPLATE_TEST(unsigned short)
SPECIALIZE_TEMPLATE_TEST(unsigned)
SPECIALIZE_TEMPLATE_TEST(unsigned long long)
SPECIALIZE_TEMPLATE_TEST(float)
SPECIALIZE_TEMPLATE_TEST(double)
SPECIALIZE_TEMPLATE_TEST(std::complex<float>)
SPECIALIZE_TEMPLATE_TEST(std::complex<double>)

SPECIALIZE_COMPLEX_1TO1_TEMPLATE_TEST(float)
SPECIALIZE_COMPLEX_1TO1_TEMPLATE_TEST(double)

}

// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef SAMPLEDVALUE_H
#define SAMPLEDVALUE_H

#include <ArduinoJson.h>
#include <memory>
#include <functional>

#include <MicroOcpp/Model/Metering/ReadingContext.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Platform.h>

namespace MicroOcpp {

template <class T>
class SampledValueDeSerializer {
public:
    static T deserialize(const char *str);
    static bool ready(T& val);
    static String serialize(T& val);
    static int32_t toInteger(T& val);
};

template <>
class SampledValueDeSerializer<int32_t> { // example class
public:
    static int32_t deserialize(const char *str);
    static bool ready(int32_t& val) {return true;} //int32_t is always valid
    static String serialize(int32_t& val);
    static int32_t toInteger(int32_t& val) {return val;} //no conversion required
};

template <>
class SampledValueDeSerializer<float> { // Used in meterValues
public:
    static float deserialize(const char *str) {return atof(str);}
    static bool ready(float& val) {return true;} //float is always valid
    static String serialize(float& val);
    static int32_t toInteger(float& val) {return (int32_t) val;}
};

class SampledValueProperties {
private:
    String format;
    String measurand;
    String phase;
    String location;
    String unit;

public:
    SampledValueProperties() :
            format(makeString("v16.Metering.SampledValueProperties")),
            measurand(makeString("v16.Metering.SampledValueProperties")),
            phase(makeString("v16.Metering.SampledValueProperties")),
            location(makeString("v16.Metering.SampledValueProperties")),
            unit(makeString("v16.Metering.SampledValueProperties")) { }
    SampledValueProperties(const SampledValueProperties& other) :
            format(other.format),
            measurand(other.measurand),
            phase(other.phase),
            location(other.location),
            unit(other.unit) { }
    ~SampledValueProperties() = default;

    void setFormat(const char *format) {this->format = format;}
    const char *getFormat() const {return format.c_str();}
    void setMeasurand(const char *measurand) {this->measurand = measurand;}
    const char *getMeasurand() const {return measurand.c_str();}
    void setPhase(const char *phase) {this->phase = phase;}
    const char *getPhase() const {return phase.c_str();}
    void setLocation(const char *location) {this->location = location;}
    const char *getLocation() const {return location.c_str();}
    void setUnit(const char *unit) {this->unit = unit;}
    const char *getUnit() const {return unit.c_str();}
};

class SampledValue {
protected:
    const SampledValueProperties& properties;
    const ReadingContext context;
    virtual String serializeValue() = 0;
public:
    SampledValue(const SampledValueProperties& properties, ReadingContext context) : properties(properties), context(context) { }
    SampledValue(const SampledValue& other) : properties(other.properties), context(other.context) { }
    virtual ~SampledValue() = default;

    std::unique_ptr<JsonDoc> toJson();

    virtual operator bool() = 0;
    virtual int32_t toInteger() = 0;

    ReadingContext getReadingContext();
};

template <class T, class DeSerializer>
class SampledValueConcrete : public SampledValue, public MemoryManaged {
private:
    T value;
public:
    SampledValueConcrete(const SampledValueProperties& properties, ReadingContext context, const T&& value) : SampledValue(properties, context), MemoryManaged("v16.Metering.SampledValueConcrete"), value(value) { }
    SampledValueConcrete(const SampledValueConcrete& other) : SampledValue(other), MemoryManaged(other), value(other.value) { }
    ~SampledValueConcrete() = default;

    operator bool() override {return DeSerializer::ready(value);}

    String serializeValue() override {return DeSerializer::serialize(value);}

    int32_t toInteger() override { return DeSerializer::toInteger(value);}
};

class SampledValueSampler {
protected:
    SampledValueProperties properties;
public:
    SampledValueSampler(SampledValueProperties properties) : properties(properties) { }
    virtual ~SampledValueSampler() = default;
    virtual std::unique_ptr<SampledValue> takeValue(ReadingContext context) = 0;
    virtual std::unique_ptr<SampledValue> deserializeValue(JsonObject svJson) = 0;
    const SampledValueProperties& getProperties() {return properties;};
};

template <class T, class DeSerializer>
class SampledValueSamplerConcrete : public SampledValueSampler, public MemoryManaged {
private:
    std::function<T(ReadingContext context)> sampler;
public:
    SampledValueSamplerConcrete(SampledValueProperties properties, std::function<T(ReadingContext)> sampler) : SampledValueSampler(properties), MemoryManaged("v16.Metering.SampledValueSamplerConcrete"), sampler(sampler) { }
    std::unique_ptr<SampledValue> takeValue(ReadingContext context) override {
        return std::unique_ptr<SampledValueConcrete<T, DeSerializer>>(new SampledValueConcrete<T, DeSerializer>(
            properties,
            context,
            sampler(context)));
    }
    std::unique_ptr<SampledValue> deserializeValue(JsonObject svJson) override {
        return std::unique_ptr<SampledValueConcrete<T, DeSerializer>>(new SampledValueConcrete<T, DeSerializer>(
            properties,
            deserializeReadingContext(svJson["context"] | "NOT_SET"),
            DeSerializer::deserialize(svJson["value"] | "")));
    }
};

} //end namespace MicroOcpp

#endif

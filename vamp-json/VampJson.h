/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Piper C++

    Centre for Digital Music, Queen Mary, University of London.
    Copyright 2015-2016 QMUL.
  
    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    Except as contained in this notice, the names of the Centre for
    Digital Music; Queen Mary, University of London; and Chris Cannam
    shall not be used in advertising or otherwise to promote the sale,
    use or other dealings in this Software without prior written
    authorization.
*/

#ifndef PIPER_VAMP_JSON_H
#define PIPER_VAMP_JSON_H

#include <vector>
#include <string>
#include <sstream>

#include <json11/json11.hpp>
#include <base-n/include/basen.hpp>

#include <vamp-hostsdk/Plugin.h>
#include <vamp-hostsdk/PluginLoader.h>
#include <vamp-hostsdk/PluginStaticData.h>
#include <vamp-hostsdk/PluginConfiguration.h>
#include <vamp-hostsdk/RequestResponse.h>

#include "vamp-support/PluginHandleMapper.h"
#include "vamp-support/PluginOutputIdMapper.h"
#include "vamp-support/RequestResponseType.h"

namespace piper {

/**
 * Convert the structures laid out in the Vamp SDK classes into JSON
 * (and back again) following the schema in the vamp-json-schema
 * project repo.
 *
 * Functions with names starting "from" convert from a Vamp SDK object
 * to JSON output. Most of them return a json11::Json object, with a
 * few exceptions for low-level utilities that return a string. These
 * functions succeed all of the time.
 *
 * Functions with names starting "to" convert to a Vamp SDK object
 * from JSON input. These functions all accept a json11::Json object
 * as first argument, with a few exceptions for low-level utilities
 * that accept a string. These functions all accept a string reference
 * as a final argument and return an error string through it if the
 * conversion fails. If conversion fails the return value is
 * undefined, and any returned object may be incomplete or
 * invalid. Callers should check for an empty error string (indicating
 * success) before using the returned value.
 */

class VampJson
{
public:
    /** Serialisation format for arrays of floats (process input and
     *  feature values). Wherever such an array appears, it may
     *  alternatively be replaced by a single string containing a
     *  base-64 encoding of the IEEE float buffer. When parsing, if a
     *  string is found instead of an array in this case, it will be
     *  interpreted as a base-64 encoded buffer. Only array or base-64
     *  encoding may be provided, not both.
     */
    enum class BufferSerialisation {

        /** Default JSON serialisation of values in array form. This
         *  is relatively slow to parse and serialise, and can take a
         *  lot of space.
         */
        Array,

        /** Base64-encoded string of the raw data as packed
         *  little-endian IEEE 32-bit floats. Faster and more compact
         *  than the text encoding but more complicated to
         *  provide. Note that Base64 serialisations produced by this
         *  library do not including padding characters and so are not
         *  necessarily multiples of 4 characters long. You will need
         *  to pad them yourself if concatenating them or supplying to
         *  a consumer that expects padding.
         */
        Base64
    };
    
    static bool failed(const std::string &err) {
        return err != "";
    }
    
    template <typename T>
    static json11::Json
    fromBasicDescriptor(const T &t) {
        return json11::Json::object { 
            { "identifier", t.identifier },
            { "name", t.name },
            { "description", t.description }
        };
    }

    template <typename T>
    static void
    toBasicDescriptor(json11::Json j, T &t, std::string &err) {
        if (!j.is_object()) {
            err = "object expected for basic descriptor content";
            return;
        }
        if (!j["identifier"].is_string()) {
            err = "string expected for identifier";
            return;
        }
        t.identifier = j["identifier"].string_value();
        t.name = j["name"].string_value();
        t.description = j["description"].string_value();
    }

    template <typename T>
    static json11::Json
    fromValueExtents(const T &t) {
        return json11::Json::object {
            { "min", t.minValue },
            { "max", t.maxValue }
        };
    }

    template <typename T>
    static bool
    toValueExtents(json11::Json j, T &t, std::string &err) {
        if (j["extents"].is_null()) {
            return false;
        } else if (j["extents"].is_object()) {
            if (j["extents"]["min"].is_number() &&
                j["extents"]["max"].is_number()) {
                t.minValue = j["extents"]["min"].number_value();
                t.maxValue = j["extents"]["max"].number_value();
                return true;
            } else {
                err = "numbers expected for min and max";
                return false;
            }
        } else {
            err = "object expected for extents (if present)";
            return false;
        }
    }

    static json11::Json
    fromRealTime(const Vamp::RealTime &r) {
        return json11::Json::object {
            { "s", r.sec },
            { "n", r.nsec }
        };
    }

    static Vamp::RealTime
    toRealTime(json11::Json j, std::string &err) {
        json11::Json sec = j["s"];
        json11::Json nsec = j["n"];
        if (!sec.is_number() || !nsec.is_number()) {
            err = "invalid Vamp::RealTime object " + j.dump();
            return {};
        }
        return Vamp::RealTime(sec.int_value(), nsec.int_value());
    }

    static std::string
    fromSampleType(Vamp::Plugin::OutputDescriptor::SampleType type) {
        switch (type) {
        case Vamp::Plugin::OutputDescriptor::OneSamplePerStep:
            return "OneSamplePerStep";
        case Vamp::Plugin::OutputDescriptor::FixedSampleRate:
            return "FixedSampleRate";
        case Vamp::Plugin::OutputDescriptor::VariableSampleRate:
            return "VariableSampleRate";
        }
        return "";
    }

    static Vamp::Plugin::OutputDescriptor::SampleType
    toSampleType(std::string text, std::string &err) {
        if (text == "OneSamplePerStep") {
            return Vamp::Plugin::OutputDescriptor::OneSamplePerStep;
        } else if (text == "FixedSampleRate") {
            return Vamp::Plugin::OutputDescriptor::FixedSampleRate;
        } else if (text == "VariableSampleRate") {
            return Vamp::Plugin::OutputDescriptor::VariableSampleRate;
        } else {
            err = "invalid sample type string: " + text;
            return Vamp::Plugin::OutputDescriptor::OneSamplePerStep;
        }
    }

    static json11::Json
    fromConfiguredOutputDescriptor(const Vamp::Plugin::OutputDescriptor &desc) {
        json11::Json::object jo {
            { "unit", desc.unit },
            { "sampleType", fromSampleType(desc.sampleType) },
            { "sampleRate", desc.sampleRate },
            { "hasDuration", desc.hasDuration }
        };
        if (desc.hasFixedBinCount) {
            jo["binCount"] = int(desc.binCount);
            jo["binNames"] = json11::Json::array
                (desc.binNames.begin(), desc.binNames.end());
        }
        if (desc.hasKnownExtents) {
            jo["extents"] = fromValueExtents(desc);
        }
        if (desc.isQuantized) {
            jo["quantizeStep"] = desc.quantizeStep;
        }
        return json11::Json(jo);
    }
    
    static json11::Json
    fromOutputDescriptor(const Vamp::Plugin::OutputDescriptor &desc) {
        json11::Json::object jo {
            { "basic", fromBasicDescriptor(desc) },
            { "configured", fromConfiguredOutputDescriptor(desc) }
        };
        return json11::Json(jo);
    }
    
    static Vamp::Plugin::OutputDescriptor
    toConfiguredOutputDescriptor(json11::Json j, std::string &err) {

        Vamp::Plugin::OutputDescriptor od;
        if (!j.is_object()) {
            err = "object expected for output descriptor";
            return {};
        }
    
        od.unit = j["unit"].string_value();

        od.sampleType = toSampleType(j["sampleType"].string_value(), err);
        if (failed(err)) return {};

        if (!j["sampleRate"].is_number()) {
            err = "number expected for sample rate";
            return {};
        }
        od.sampleRate = j["sampleRate"].number_value();
        od.hasDuration = j["hasDuration"].bool_value();

        if (j["binCount"].is_number() && j["binCount"].int_value() > 0) {
            od.hasFixedBinCount = true;
            od.binCount = j["binCount"].int_value();
            for (auto &n: j["binNames"].array_items()) {
                if (!n.is_string()) {
                    err = "string expected for bin name";
                    return {};
                }
                od.binNames.push_back(n.string_value());
            }
        } else {
            od.hasFixedBinCount = false;
        }

        bool extentsPresent = toValueExtents(j, od, err);
        if (failed(err)) return {};
        
        od.hasKnownExtents = extentsPresent;

        if (j["quantizeStep"].is_number()) {
            od.isQuantized = true;
            od.quantizeStep = j["quantizeStep"].number_value();
        } else {
            od.isQuantized = false;
        }

        return od;
    }
    
    static Vamp::Plugin::OutputDescriptor
    toOutputDescriptor(json11::Json j, std::string &err) {

        Vamp::Plugin::OutputDescriptor od;
        if (!j.is_object()) {
            err = "object expected for output descriptor";
            return {};
        }

        od = toConfiguredOutputDescriptor(j, err);
        if (failed(err)) return {};
    
        toBasicDescriptor(j["basic"], od, err);
        if (failed(err)) return {};

        return od;
    }

    static json11::Json
    fromParameterDescriptor(const Vamp::PluginBase::ParameterDescriptor &desc) {

        json11::Json::object jo {
            { "basic", fromBasicDescriptor(desc) },
            { "unit", desc.unit },
            { "extents", fromValueExtents(desc) },
            { "defaultValue", desc.defaultValue },
            { "valueNames", json11::Json::array
                    (desc.valueNames.begin(), desc.valueNames.end()) }
        };
        if (desc.isQuantized) {
            jo["quantizeStep"] = desc.quantizeStep;
        }
        return json11::Json(jo);
    }

    static Vamp::PluginBase::ParameterDescriptor
    toParameterDescriptor(json11::Json j, std::string &err) {

        Vamp::PluginBase::ParameterDescriptor pd;
        if (!j.is_object()) {
            err = "object expected for parameter descriptor";
            return {};
        }
    
        toBasicDescriptor(j["basic"], pd, err);
        if (failed(err)) return {};

        pd.unit = j["unit"].string_value();

        bool extentsPresent = toValueExtents(j, pd, err);
        if (failed(err)) return {};
        if (!extentsPresent) {
            err = "extents must be present in parameter descriptor";
            return {};
        }
    
        if (!j["defaultValue"].is_number()) {
            err = "number expected for default value";
            return {};
        }
    
        pd.defaultValue = j["defaultValue"].number_value();

        pd.valueNames.clear();
        for (auto &n: j["valueNames"].array_items()) {
            if (!n.is_string()) {
                err = "string expected for value name";
                return {};
            }
            pd.valueNames.push_back(n.string_value());
        }

        if (j["quantizeStep"].is_number()) {
            pd.isQuantized = true;
            pd.quantizeStep = j["quantizeStep"].number_value();
        } else {
            pd.isQuantized = false;
        }

        return pd;
    }

    static std::string
    fromFloatBuffer(const float *buffer, size_t nfloats) {
        // must use char pointers, otherwise the converter will only
        // encode every 4th byte (as it will count up in float* steps)
        const char *start = reinterpret_cast<const char *>(buffer);
        const char *end = reinterpret_cast<const char *>(buffer + nfloats);
        std::string encoded;
        bn::encode_b64(start, end, back_inserter(encoded));
        return encoded;
    }

    static std::vector<float>
    toFloatBuffer(std::string encoded, std::string & /* err */) {
        std::string decoded;
        bn::decode_b64(encoded.begin(), encoded.end(), back_inserter(decoded));
        const float *buffer = reinterpret_cast<const float *>(decoded.c_str());
        size_t n = decoded.size() / sizeof(float);
        return std::vector<float>(buffer, buffer + n);
    }

    static json11::Json
    fromFeature(const Vamp::Plugin::Feature &f,
                BufferSerialisation serialisation) {

        json11::Json::object jo;
        if (f.values.size() > 0) {
            if (serialisation == BufferSerialisation::Array) {
                jo["featureValues"] = json11::Json::array(f.values.begin(),
                                                          f.values.end());
            } else {
                jo["featureValues"] = fromFloatBuffer(f.values.data(),
                                                      f.values.size());
            }
        }
        if (f.label != "") {
            jo["label"] = f.label;
        }
        if (f.hasTimestamp) {
            jo["timestamp"] = fromRealTime(f.timestamp);
        }
        if (f.hasDuration) {
            jo["duration"] = fromRealTime(f.duration);
        }
        return json11::Json(jo);
    }

    static Vamp::Plugin::Feature
    toFeature(json11::Json j, BufferSerialisation &serialisation, std::string &err) {

        Vamp::Plugin::Feature f;
        if (!j.is_object()) {
            err = "object expected for feature";
            return {};
        }
        if (j["timestamp"].is_object()) {
            f.timestamp = toRealTime(j["timestamp"], err);
            if (failed(err)) return {};
            f.hasTimestamp = true;
        }
        if (j["duration"].is_object()) {
            f.duration = toRealTime(j["duration"], err);
            if (failed(err)) return {};
            f.hasDuration = true;
        }
        if (j["featureValues"].is_string()) {
            f.values = toFloatBuffer(j["featureValues"].string_value(), err);
            if (failed(err)) return {};
            serialisation = BufferSerialisation::Base64;
        } else if (j["featureValues"].is_array()) {
            for (auto v : j["featureValues"].array_items()) {
                f.values.push_back(v.number_value());
            }
            serialisation = BufferSerialisation::Array;
        }
        f.label = j["label"].string_value();
        return f;
    }

    static json11::Json
    fromFeatureSet(const Vamp::Plugin::FeatureSet &fs,
                   const PluginOutputIdMapper &omapper,
                   BufferSerialisation serialisation) {

        json11::Json::object jo;
        for (const auto &fsi : fs) {
            std::vector<json11::Json> fj;
            for (const Vamp::Plugin::Feature &f: fsi.second) {
                fj.push_back(fromFeature(f, serialisation));
            }
            jo[omapper.indexToId(fsi.first)] = fj;
        }
        return json11::Json(jo);
    }

    static Vamp::Plugin::FeatureList
    toFeatureList(json11::Json j,
                  BufferSerialisation &serialisation, std::string &err) {

        Vamp::Plugin::FeatureList fl;
        if (!j.is_array()) {
            err = "array expected for feature list";
            return {};
        }
        for (const json11::Json &fj : j.array_items()) {
            fl.push_back(toFeature(fj, serialisation, err));
            if (failed(err)) return {};
        }
        return fl;
    }

    static Vamp::Plugin::FeatureSet
    toFeatureSet(json11::Json j,
                 const PluginOutputIdMapper &omapper,
                 BufferSerialisation &serialisation,
                 std::string &err) {

        Vamp::Plugin::FeatureSet fs;
        if (!j.is_object()) {
            err = "object expected for feature set";
            return {};
        }
        for (auto &entry : j.object_items()) {
            int n = omapper.idToIndex(entry.first);
            if (fs.find(n) != fs.end()) {
                err = "duplicate numerical index for output";
                return {};
            }
            fs[n] = toFeatureList(entry.second, serialisation, err);
            if (failed(err)) return {};
        }
        return fs;
    }

    static std::string
    fromInputDomain(Vamp::Plugin::InputDomain domain) {

        switch (domain) {
        case Vamp::Plugin::TimeDomain:
            return "TimeDomain";
        case Vamp::Plugin::FrequencyDomain:
            return "FrequencyDomain";
        }
        return "";
    }

    static Vamp::Plugin::InputDomain
    toInputDomain(std::string text, std::string &err) {

        if (text == "TimeDomain") {
            return Vamp::Plugin::TimeDomain;
        } else if (text == "FrequencyDomain") {
            return Vamp::Plugin::FrequencyDomain;
        } else {
            err = "invalid input domain string: " + text;
            return {};
        }
    }

    static json11::Json
    fromPluginStaticData(const Vamp::HostExt::PluginStaticData &d) {

        json11::Json::object jo;
        jo["key"] = d.pluginKey;
        jo["basic"] = fromBasicDescriptor(d.basic);
        jo["maker"] = d.maker;
        jo["copyright"] = d.copyright;
        jo["version"] = d.pluginVersion;

        json11::Json::array cat;
        for (const std::string &c: d.category) cat.push_back(c);
        jo["category"] = cat;

        jo["minChannelCount"] = d.minChannelCount;
        jo["maxChannelCount"] = d.maxChannelCount;

        json11::Json::array params;
        Vamp::PluginBase::ParameterList vparams = d.parameters;
        for (auto &p: vparams) params.push_back(fromParameterDescriptor(p));
        jo["parameters"] = params;

        json11::Json::array progs;
        Vamp::PluginBase::ProgramList vprogs = d.programs;
        for (auto &p: vprogs) progs.push_back(p);
        jo["programs"] = progs;

        jo["inputDomain"] = fromInputDomain(d.inputDomain);

        json11::Json::array outinfo;
        auto vouts = d.basicOutputInfo;
        for (auto &o: vouts) outinfo.push_back(fromBasicDescriptor(o));
        jo["basicOutputInfo"] = outinfo;
    
        return json11::Json(jo);
    }

    static Vamp::HostExt::PluginStaticData
    toPluginStaticData(json11::Json j, std::string &err) {

        if (!j.has_shape({
                    { "key", json11::Json::STRING },
                    { "version", json11::Json::NUMBER },
                    { "minChannelCount", json11::Json::NUMBER },
                    { "maxChannelCount", json11::Json::NUMBER },
                    { "inputDomain", json11::Json::STRING }}, err)) {

            err = "malformed plugin static data: " + err;

        } else if (!j["basicOutputInfo"].is_array()) {

            err = "array expected for basic output info";

        } else if (!j["maker"].is_null() &&
                   !j["maker"].is_string()) {

            err = "string expected for maker";

        } else if (!j["copyright"].is_null() &&
                   !j["copyright"].is_string()) {
            err = "string expected for copyright";

        } else if (!j["category"].is_null() &&
                   !j["category"].is_array()) {

            err = "array expected for category";

        } else if (!j["parameters"].is_null() &&
                   !j["parameters"].is_array()) {

            err = "array expected for parameters";

        } else if (!j["programs"].is_null() &&
                   !j["programs"].is_array()) {

            err = "array expected for programs";

        } else if (!j["inputDomain"].is_null() &&
                   !j["inputDomain"].is_string()) {

            err = "string expected for inputDomain";

        } else if (!j["basicOutputInfo"].is_null() &&
                   !j["basicOutputInfo"].is_array()) {
            
            err = "array expected for basicOutputInfo";

        } else {

            Vamp::HostExt::PluginStaticData psd;

            psd.pluginKey = j["key"].string_value();

            toBasicDescriptor(j["basic"], psd.basic, err);
            if (failed(err)) return {};

            psd.maker = j["maker"].string_value();
            psd.copyright = j["copyright"].string_value();
            psd.pluginVersion = j["version"].int_value();

            for (const auto &c : j["category"].array_items()) {
                if (!c.is_string()) {
                    err = "strings expected in category array";
                    return {};
                }
                psd.category.push_back(c.string_value());
            }

            psd.minChannelCount = j["minChannelCount"].int_value();
            psd.maxChannelCount = j["maxChannelCount"].int_value();

            for (const auto &p : j["parameters"].array_items()) {
                auto pd = toParameterDescriptor(p, err);
                if (failed(err)) return {};
                psd.parameters.push_back(pd);
            }

            for (const auto &p : j["programs"].array_items()) {
                if (!p.is_string()) {
                    err = "strings expected in programs array";
                    return {};
                }
                psd.programs.push_back(p.string_value());
            }

            psd.inputDomain = toInputDomain(j["inputDomain"].string_value(), err);
            if (failed(err)) return {};

            for (const auto &bo : j["basicOutputInfo"].array_items()) {
                Vamp::HostExt::PluginStaticData::Basic b;
                toBasicDescriptor(bo, b, err);
                if (failed(err)) return {};
                psd.basicOutputInfo.push_back(b);
            }
            
            return psd;
        }

        // fallthrough error case
        return {};
    }

    static json11::Json
    fromPluginConfiguration(const Vamp::HostExt::PluginConfiguration &c) {

        json11::Json::object jo;

        json11::Json::object paramValues;
        for (auto &vp: c.parameterValues) {
            paramValues[vp.first] = vp.second;
        }
        jo["parameterValues"] = paramValues;

        if (c.currentProgram != "") {
            jo["currentProgram"] = c.currentProgram;
        }

        jo["channelCount"] = c.channelCount;
        jo["stepSize"] = c.stepSize;
        jo["blockSize"] = c.blockSize;
    
        return json11::Json(jo);
    }

    static Vamp::HostExt::PluginConfiguration
    toPluginConfiguration(json11::Json j, std::string &err) {
        
        if (!j.has_shape({
                    { "channelCount", json11::Json::NUMBER },
                    { "stepSize", json11::Json::NUMBER },
                    { "blockSize", json11::Json::NUMBER } }, err)) {
            err = "malformed plugin configuration: " + err;
            return {};
        }

        if (!j["parameterValues"].is_null() &&
            !j["parameterValues"].is_object()) {
            err = "object expected for parameter values";
            return {};
        }

        for (auto &pv : j["parameterValues"].object_items()) {
            if (!pv.second.is_number()) {
                err = "number expected for parameter value";
                return {};
            }
        }
    
        if (!j["currentProgram"].is_null() &&
            !j["currentProgram"].is_string()) {
            err = "string expected for program name";
            return {};
        }

        Vamp::HostExt::PluginConfiguration config;

        config.channelCount = j["channelCount"].number_value();
        config.stepSize = j["stepSize"].number_value();
        config.blockSize = j["blockSize"].number_value();
        
        for (auto &pv : j["parameterValues"].object_items()) {
            config.parameterValues[pv.first] = pv.second.number_value();
        }

        if (j["currentProgram"].is_string()) {
            config.currentProgram = j["currentProgram"].string_value();
        }

        return config;
    }

    static json11::Json
    fromAdapterFlags(int flags) {

        json11::Json::array arr;

        if (flags & Vamp::HostExt::PluginLoader::ADAPT_INPUT_DOMAIN) {
            arr.push_back("AdaptInputDomain");
        }
        if (flags & Vamp::HostExt::PluginLoader::ADAPT_CHANNEL_COUNT) {
            arr.push_back("AdaptChannelCount");
        }
        if (flags & Vamp::HostExt::PluginLoader::ADAPT_BUFFER_SIZE) {
            arr.push_back("AdaptBufferSize");
        }

        return json11::Json(arr);
    }

    static Vamp::HostExt::PluginLoader::AdapterFlags
    toAdapterFlags(json11::Json j, std::string &err) {

        int flags = 0x0;

        if (!j.is_array()) {

            err = "array expected for adapter flags";

        } else {

            for (auto &jj: j.array_items()) {
                if (!jj.is_string()) {
                    err = "string expected for adapter flag";
                    break;
                }
                std::string text = jj.string_value();
                if (text == "AdaptInputDomain") {
                    flags |= Vamp::HostExt::PluginLoader::ADAPT_INPUT_DOMAIN;
                } else if (text == "AdaptChannelCount") {
                    flags |= Vamp::HostExt::PluginLoader::ADAPT_CHANNEL_COUNT;
                } else if (text == "AdaptBufferSize") {
                    flags |= Vamp::HostExt::PluginLoader::ADAPT_BUFFER_SIZE;
                } else if (text == "AdaptAllSafe") {
                    flags |= Vamp::HostExt::PluginLoader::ADAPT_ALL_SAFE;
                } else if (text == "AdaptAll") {
                    flags |= Vamp::HostExt::PluginLoader::ADAPT_ALL;
                } else {
                    err = "invalid adapter flag string: " + text;
                    break;
                }
            }
        }

        return Vamp::HostExt::PluginLoader::AdapterFlags(flags);
    }

    static json11::Json
    fromLoadRequest(const Vamp::HostExt::LoadRequest &req) {

        json11::Json::object jo;
        jo["key"] = req.pluginKey;
        jo["inputSampleRate"] = req.inputSampleRate;
        jo["adapterFlags"] = fromAdapterFlags(req.adapterFlags);
        return json11::Json(jo);
    }

    static Vamp::HostExt::LoadRequest
    toLoadRequest(json11::Json j, std::string &err) {
        
        if (!j.has_shape({
                    { "key", json11::Json::STRING },
                    { "inputSampleRate", json11::Json::NUMBER } }, err)) {
            err = "malformed load request: " + err;
            return {};
        }
    
        Vamp::HostExt::LoadRequest req;
        req.pluginKey = j["key"].string_value();
        req.inputSampleRate = j["inputSampleRate"].number_value();
        if (!j["adapterFlags"].is_null()) {
            req.adapterFlags = toAdapterFlags(j["adapterFlags"], err);
            if (failed(err)) return {};
        }
        return req;
    }

    static json11::Json
    fromLoadResponse(const Vamp::HostExt::LoadResponse &resp,
                     const PluginHandleMapper &pmapper) {

        json11::Json::object jo;
        jo["handle"] = double(pmapper.pluginToHandle(resp.plugin));
        jo["staticData"] = fromPluginStaticData(resp.staticData);
        jo["defaultConfiguration"] =
            fromPluginConfiguration(resp.defaultConfiguration);
        return json11::Json(jo);
    }

    static Vamp::HostExt::LoadResponse
    toLoadResponse(json11::Json j,
                   const PluginHandleMapper &pmapper, std::string &err) {

        if (!j.has_shape({
                    { "handle", json11::Json::NUMBER },
                    { "staticData", json11::Json::OBJECT },
                    { "defaultConfiguration", json11::Json::OBJECT } }, err)) {
            err = "malformed load response: " + err;
            return {};
        }

        Vamp::HostExt::LoadResponse resp;
        resp.plugin = pmapper.handleToPlugin(j["handle"].int_value());
        resp.staticData = toPluginStaticData(j["staticData"], err);
        if (failed(err)) return {};
        resp.defaultConfiguration = toPluginConfiguration(j["defaultConfiguration"],
                                                          err);
        if (failed(err)) return {};
        return resp;
    }

    static json11::Json
    fromConfigurationRequest(const Vamp::HostExt::ConfigurationRequest &cr,
                             const PluginHandleMapper &pmapper) {

        json11::Json::object jo;

        jo["handle"] = double(pmapper.pluginToHandle(cr.plugin));
        jo["configuration"] = fromPluginConfiguration(cr.configuration);
        
        return json11::Json(jo);
    }

    static Vamp::HostExt::ConfigurationRequest
    toConfigurationRequest(json11::Json j,
                           const PluginHandleMapper &pmapper, std::string &err) {

        if (!j.has_shape({
                    { "handle", json11::Json::NUMBER },
                    { "configuration", json11::Json::OBJECT } }, err)) {
            err = "malformed configuration request: " + err;
            return {};
        }

        Vamp::HostExt::ConfigurationRequest cr;
        cr.plugin = pmapper.handleToPlugin(j["handle"].int_value());
        cr.configuration = toPluginConfiguration(j["configuration"], err);
        if (failed(err)) return {};
        return cr;
    }

    static json11::Json
    fromConfigurationResponse(const Vamp::HostExt::ConfigurationResponse &cr,
                              const PluginHandleMapper &pmapper) {

        json11::Json::object jo;

        jo["handle"] = double(pmapper.pluginToHandle(cr.plugin));
        
        json11::Json::array outs;
        for (auto &d: cr.outputs) {
            outs.push_back(fromOutputDescriptor(d));
        }
        jo["outputList"] = outs;
        
        return json11::Json(jo);
    }

    static Vamp::HostExt::ConfigurationResponse
    toConfigurationResponse(json11::Json j,
                            const PluginHandleMapper &pmapper, std::string &err) {
        
        Vamp::HostExt::ConfigurationResponse cr;

        cr.plugin = pmapper.handleToPlugin(j["handle"].int_value());
        
        if (!j["outputList"].is_array()) {
            err = "array expected for output list";
            return {};
        }

        for (const auto &o: j["outputList"].array_items()) {
            cr.outputs.push_back(toOutputDescriptor(o, err));
            if (failed(err)) return {};
        }

        return cr;
    }

    static json11::Json
    fromProcessRequest(const Vamp::HostExt::ProcessRequest &r,
                       const PluginHandleMapper &pmapper,
                       BufferSerialisation serialisation) {

        json11::Json::object jo;
        jo["handle"] = double(pmapper.pluginToHandle(r.plugin));

        json11::Json::object io;
        io["timestamp"] = fromRealTime(r.timestamp);

        json11::Json::array chans;
        for (size_t i = 0; i < r.inputBuffers.size(); ++i) {
            if (serialisation == BufferSerialisation::Array) {
                chans.push_back(json11::Json::array(r.inputBuffers[i].begin(),
                                                    r.inputBuffers[i].end()));
            } else {
                chans.push_back(fromFloatBuffer(r.inputBuffers[i].data(),
                                                r.inputBuffers[i].size()));
            }
        }
        io["inputBuffers"] = chans;
        
        jo["processInput"] = io;
        return json11::Json(jo);
    }

    static Vamp::HostExt::ProcessRequest
    toProcessRequest(json11::Json j,
                     const PluginHandleMapper &pmapper,
                     BufferSerialisation &serialisation, std::string &err) {

        if (!j.has_shape({
                    { "handle", json11::Json::NUMBER },
                    { "processInput", json11::Json::OBJECT } }, err)) {
            err = "malformed process request: " + err;
            return {};
        }

        auto input = j["processInput"];

        if (!input.has_shape({
                    { "timestamp", json11::Json::OBJECT },
                    { "inputBuffers", json11::Json::ARRAY } }, err)) {
            err = "malformed process request: " + err;
            return {};
        }

        Vamp::HostExt::ProcessRequest r;
        r.plugin = pmapper.handleToPlugin(j["handle"].int_value());

        r.timestamp = toRealTime(input["timestamp"], err);
        if (failed(err)) return {};

        for (const auto &a: input["inputBuffers"].array_items()) {

            if (a.is_string()) {
                std::vector<float> buf = toFloatBuffer(a.string_value(),
                                                       err);
                if (failed(err)) return {};
                r.inputBuffers.push_back(buf);
                serialisation = BufferSerialisation::Base64;

            } else if (a.is_array()) {
                std::vector<float> buf;
                for (auto v : a.array_items()) {
                    buf.push_back(v.number_value());
                }
                r.inputBuffers.push_back(buf);
                serialisation = BufferSerialisation::Array;

            } else {
                err = "expected arrays or strings in inputBuffers array";
                return {};
            }
        }

        return r;
    }
    
private: // go private briefly for a couple of helper functions
    
    static void
    checkTypeField(json11::Json j, std::string expected, std::string &err) {
        if (!j["method"].is_string()) {
            err = "string expected for method";
            return;
        }
        if (j["method"].string_value() != expected) {
            err = "expected value \"" + expected + "\" for type";
            return;
        }
    }

    static bool
    successful(json11::Json j, std::string &err) {
        if (!j["success"].is_bool()) {
            err = "bool expected for success";
            return false;
        }
        return j["success"].bool_value();
    }

    static void
    markRPC(json11::Json::object &jo) {
        jo["jsonrpc"] = "2.0";
    }

    static void
    addId(json11::Json::object &jo, const json11::Json &id) {
        if (!id.is_null()) {
            jo["id"] = id;
        }
    }
    
public:

    static json11::Json
    fromRpcRequest_List(const json11::Json &id) {

        json11::Json::object jo;
        markRPC(jo);

        jo["method"] = "list";
        addId(jo, id);
        return json11::Json(jo);
    }

    static json11::Json
    fromRpcResponse_List(const Vamp::HostExt::ListResponse &resp,
                         const json11::Json &id) {

        json11::Json::object jo;
        markRPC(jo);

        json11::Json::array arr;
        for (const auto &a: resp.available) {
            arr.push_back(fromPluginStaticData(a));
        }
        json11::Json::object po;
        po["available"] = arr;

        jo["method"] = "list";
        jo["result"] = po;
        addId(jo, id);
        return json11::Json(jo);
    }
    
    static json11::Json
    fromRpcRequest_Load(const Vamp::HostExt::LoadRequest &req,
                        const json11::Json &id) {

        json11::Json::object jo;
        markRPC(jo);

        jo["method"] = "load";
        jo["params"] = fromLoadRequest(req);
        addId(jo, id);
        return json11::Json(jo);
    }    

    static json11::Json
    fromRpcResponse_Load(const Vamp::HostExt::LoadResponse &resp,
                         const PluginHandleMapper &pmapper,
                         const json11::Json &id) {

        if (resp.plugin) {

            json11::Json::object jo;
            markRPC(jo);

            jo["method"] = "load";
            jo["result"] = fromLoadResponse(resp, pmapper);
            addId(jo, id);
            return json11::Json(jo);
            
        } else {
            return fromError("Failed to load plugin", RRType::Load, id);
        }
    }

    static json11::Json
    fromRpcRequest_Configure(const Vamp::HostExt::ConfigurationRequest &req,
                             const PluginHandleMapper &pmapper,
                             const json11::Json &id) {

        json11::Json::object jo;
        markRPC(jo);

        jo["method"] = "configure";
        jo["params"] = fromConfigurationRequest(req, pmapper);
        addId(jo, id);
        return json11::Json(jo);
    }    

    static json11::Json
    fromRpcResponse_Configure(const Vamp::HostExt::ConfigurationResponse &resp,
                              const PluginHandleMapper &pmapper,
                              const json11::Json &id) {

        if (!resp.outputs.empty()) {
        
            json11::Json::object jo;
            markRPC(jo);

            jo["method"] = "configure";
            jo["result"] = fromConfigurationResponse(resp, pmapper);
            addId(jo, id);
            return json11::Json(jo);

        } else {
            return fromError("Failed to configure plugin", RRType::Configure, id);
        }
    }
    
    static json11::Json
    fromRpcRequest_Process(const Vamp::HostExt::ProcessRequest &req,
                           const PluginHandleMapper &pmapper,
                           BufferSerialisation serialisation,
                           const json11::Json &id) {

        json11::Json::object jo;
        markRPC(jo);

        jo["method"] = "process";
        jo["params"] = fromProcessRequest(req, pmapper, serialisation);
        addId(jo, id);
        return json11::Json(jo);
    }    

    static json11::Json
    fromRpcResponse_Process(const Vamp::HostExt::ProcessResponse &resp,
                            const PluginHandleMapper &pmapper,
                            BufferSerialisation serialisation,
                            const json11::Json &id) {
        
        json11::Json::object jo;
        markRPC(jo);

        json11::Json::object po;
        po["handle"] = double(pmapper.pluginToHandle(resp.plugin));
        po["features"] = fromFeatureSet(resp.features,
                                        *pmapper.pluginToOutputIdMapper(resp.plugin),
                                        serialisation);
        jo["method"] = "process";
        jo["result"] = po;
        addId(jo, id);
        return json11::Json(jo);
    }
    
    static json11::Json
    fromRpcRequest_Finish(const Vamp::HostExt::FinishRequest &req,
                          const PluginHandleMapper &pmapper,
                          const json11::Json &id) {

        json11::Json::object jo;
        markRPC(jo);

        json11::Json::object fo;
        fo["handle"] = double(pmapper.pluginToHandle(req.plugin));

        jo["method"] = "finish";
        jo["params"] = fo;
        addId(jo, id);
        return json11::Json(jo);
    }    
    
    static json11::Json
    fromRpcResponse_Finish(const Vamp::HostExt::ProcessResponse &resp,
                           const PluginHandleMapper &pmapper,
                           BufferSerialisation serialisation,
                           const json11::Json &id) {

        json11::Json::object jo;
        markRPC(jo);

        json11::Json::object po;
        po["handle"] = double(pmapper.pluginToHandle(resp.plugin));
        po["features"] = fromFeatureSet(resp.features,
                                        *pmapper.pluginToOutputIdMapper(resp.plugin),
                                        serialisation);
        jo["method"] = "finish";
        jo["result"] = po;
        addId(jo, id);
        return json11::Json(jo);
    }

    static json11::Json
    fromError(std::string errorText,
              RRType responseType,
              const json11::Json &id) {

        json11::Json::object jo;
        markRPC(jo);

        std::string type;

        if (responseType == RRType::List) type = "list";
        else if (responseType == RRType::Load) type = "load";
        else if (responseType == RRType::Configure) type = "configure";
        else if (responseType == RRType::Process) type = "process";
        else if (responseType == RRType::Finish) type = "finish";
        else type = "invalid";

        json11::Json::object eo;
        eo["code"] = 0;
        eo["message"] = 
            std::string("error in ") + type + " request: " + errorText;

        jo["method"] = type;
        jo["error"] = eo;
        addId(jo, id);
        return json11::Json(jo);
    }

    static RRType
    getRequestResponseType(json11::Json j, std::string &err) {

        if (!j["method"].is_string()) {
            err = "string expected for method";
            return RRType::NotValid;
        }
        
        std::string type = j["method"].string_value();

	if (type == "list") return RRType::List;
	else if (type == "load") return RRType::Load;
	else if (type == "configure") return RRType::Configure;
	else if (type == "process") return RRType::Process;
	else if (type == "finish") return RRType::Finish;
        else if (type == "invalid") return RRType::NotValid;
	else {
	    err = "unknown or unexpected request/response type \"" + type + "\"";
            return RRType::NotValid;
	}
    }

    static void
    toRpcRequest_List(json11::Json j, std::string &err) {
        checkTypeField(j, "list", err);
    }

    static Vamp::HostExt::ListResponse
    toRpcResponse_List(json11::Json j, std::string &err) {

        Vamp::HostExt::ListResponse resp;
        if (successful(j, err) && !failed(err)) {
            for (const auto &a: j["result"]["available"].array_items()) {
                resp.available.push_back(toPluginStaticData(a, err));
                if (failed(err)) return {};
            }
        }
        
        return resp;
    }

    static Vamp::HostExt::LoadRequest
    toRpcRequest_Load(json11::Json j, std::string &err) {
        
        checkTypeField(j, "load", err);
        if (failed(err)) return {};
        return toLoadRequest(j["params"], err);
    }
    
    static Vamp::HostExt::LoadResponse
    toRpcResponse_Load(json11::Json j,
                        const PluginHandleMapper &pmapper,
                        std::string &err) {
        
        Vamp::HostExt::LoadResponse resp;
        if (successful(j, err) && !failed(err)) {
            resp = toLoadResponse(j["result"], pmapper, err);
        }
        return resp;
    }
    
    static Vamp::HostExt::ConfigurationRequest
    toRpcRequest_Configure(json11::Json j,
                            const PluginHandleMapper &pmapper,
                            std::string &err) {
        
        checkTypeField(j, "configure", err);
        if (failed(err)) return {};
        return toConfigurationRequest(j["params"], pmapper, err);
    }
    
    static Vamp::HostExt::ConfigurationResponse
    toRpcResponse_Configure(json11::Json j,
                             const PluginHandleMapper &pmapper,
                             std::string &err) {
        
        Vamp::HostExt::ConfigurationResponse resp;
        if (successful(j, err) && !failed(err)) {
            resp = toConfigurationResponse(j["result"], pmapper, err);
        }
        return resp;
    }
    
    static Vamp::HostExt::ProcessRequest
    toRpcRequest_Process(json11::Json j, const PluginHandleMapper &pmapper,
                          BufferSerialisation &serialisation, std::string &err) {
        
        checkTypeField(j, "process", err);
        if (failed(err)) return {};
        return toProcessRequest(j["params"], pmapper, serialisation, err);
    }
    
    static Vamp::HostExt::ProcessResponse
    toRpcResponse_Process(json11::Json j,
                           const PluginHandleMapper &pmapper,
                           BufferSerialisation &serialisation, std::string &err) {
        
        Vamp::HostExt::ProcessResponse resp;
        if (successful(j, err) && !failed(err)) {
            auto jc = j["result"];
            auto h = jc["handle"].int_value();
            resp.plugin = pmapper.handleToPlugin(h);
            resp.features = toFeatureSet(jc["features"],
                                         *pmapper.handleToOutputIdMapper(h),
                                         serialisation, err);
        }
        return resp;
    }
    
    static Vamp::HostExt::FinishRequest
    toRpcRequest_Finish(json11::Json j, const PluginHandleMapper &pmapper,
                         std::string &err) {
        
        checkTypeField(j, "finish", err);
        if (failed(err)) return {};
        Vamp::HostExt::FinishRequest req;
        req.plugin = pmapper.handleToPlugin
            (j["params"]["handle"].int_value());
        return req;
    }
    
    static Vamp::HostExt::ProcessResponse
    toRpcResponse_Finish(json11::Json j,
                          const PluginHandleMapper &pmapper,
                          BufferSerialisation &serialisation, std::string &err) {
        
        Vamp::HostExt::ProcessResponse resp;
        if (successful(j, err) && !failed(err)) {
            auto jc = j["result"];
            auto h = jc["handle"].int_value();
            resp.plugin = pmapper.handleToPlugin(h);
            resp.features = toFeatureSet(jc["features"],
                                         *pmapper.handleToOutputIdMapper(h),
                                         serialisation, err);
        }
        return resp;
    }
};

}

#endif

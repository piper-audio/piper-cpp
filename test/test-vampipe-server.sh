#!/bin/bash 

set -eu

reqfile="/tmp/$$.req.json"
respfile="/tmp/$$.resp.json"
allrespfile="/tmp/$$.resp.all"
expected="/tmp/$$.expected"
obtained="/tmp/$$.obtained"
trap "rm -f $reqfile $respfile $allrespfile $obtained $expected" 0

schema=vamp-json-schema/schema

validate() {
    local file="$1"
    local schemaname="$2"
    jsonschema -i "$file" "$schema/$schemaname.json" 1>&2 && \
        echo "validated $schemaname" 1>&2 || \
        echo "failed to validate $schemaname" 1>&2
}

validate_request() {
    local json="$1"
    echo "$json" > "$reqfile"
    validate "$reqfile" "rpcrequest"
}

validate_response() {
    local json="$1"
    echo "$json" > "$respfile"
    validate "$respfile" "rpcresponse"
}

( while read request ; do
      validate_request "$request"
      echo "$request"
  done |
      bin/vampipe-convert request -i json -o capnp |
      VAMP_PATH=./vamp-plugin-sdk/examples bin/vampipe-server |
      bin/vampipe-convert response -i capnp -o json |
      while read response ; do
          echo "$response" >> "$allrespfile"
          validate_response "$response"
      done
) <<EOF
{"method":"list"}
{"method":"load","id":6,"params": {"key":"vamp-example-plugins:percussiononsets","inputSampleRate":44100,"adapterFlags":["AdaptInputDomain","AdaptBufferSize"]}}
{"method":"configure","id":"weevil","params":{"handle":1,"configuration":{"blockSize": 8, "channelCount": 1, "parameterValues": {"sensitivity": 40, "threshold": 3}, "stepSize": 8}}}
{"method":"process","params": {"handle": 1, "processInput": { "timestamp": {"s": 0, "n": 0}, "inputBuffers": [ [1,2,3,4,5,6,7,8] ]}}}
{"method":"finish","params": {"handle": 1}}
EOF

# Expected output, apart from the plugin list which seems a bit
# fragile to check here
cat > "$expected" <<EOF
{"id": 6, "jsonrpc": "2.0", "method": "load", "result": {"defaultConfiguration": {"blockSize": 1024, "channelCount": 1, "parameterValues": {"sensitivity": 40, "threshold": 3}, "stepSize": 1024}, "handle": 1, "staticData": {"basic": {"description": "Detect percussive note onsets by identifying broadband energy rises", "identifier": "percussiononsets", "name": "Simple Percussion Onset Detector"}, "basicOutputInfo": [{"description": "Percussive note onset locations", "identifier": "onsets", "name": "Onsets"}, {"description": "Broadband energy rise detection function", "identifier": "detectionfunction", "name": "Detection Function"}], "category": ["Time", "Onsets"], "copyright": "Code copyright 2006 Queen Mary, University of London, after Dan Barry et al 2005.  Freely redistributable (BSD license)", "inputDomain": "TimeDomain", "key": "vamp-example-plugins:percussiononsets", "maker": "Vamp SDK Example Plugins", "maxChannelCount": 1, "minChannelCount": 1, "parameters": [{"basic": {"description": "Energy rise within a frequency bin necessary to count toward broadband total", "identifier": "threshold", "name": "Energy rise threshold"}, "defaultValue": 3, "extents": {"max": 20, "min": 0}, "unit": "dB", "valueNames": []}, {"basic": {"description": "Sensitivity of peak detector applied to broadband detection function", "identifier": "sensitivity", "name": "Sensitivity"}, "defaultValue": 40, "extents": {"max": 100, "min": 0}, "unit": "%", "valueNames": []}], "programs": [], "version": 2}}}
{"id": "weevil", "jsonrpc": "2.0", "method": "configure", "result": {"handle": 1, "outputList": [{"basic": {"description": "Percussive note onset locations", "identifier": "onsets", "name": "Onsets"}, "configured": {"binCount": 0, "binNames": [], "hasDuration": false, "sampleRate": 44100, "sampleType": "VariableSampleRate", "unit": ""}}, {"basic": {"description": "Broadband energy rise detection function", "identifier": "detectionfunction", "name": "Detection Function"}, "configured": {"binCount": 1, "binNames": [""], "hasDuration": false, "quantizeStep": 1, "sampleRate": 86.1328125, "sampleType": "FixedSampleRate", "unit": ""}}]}}
{"jsonrpc": "2.0", "method": "process", "result": {"features": {}, "handle": 1}}
{"jsonrpc": "2.0", "method": "finish", "result": {"features": {"detectionfunction": [{"featureValues": [0], "timestamp": {"n": 11609977, "s": 0}}]}, "handle": 1}}
EOF

# Skip plugin list
tail -n +2 "$allrespfile" > "$obtained"

echo "Checking response contents against expected contents..."
if ! cmp "$obtained" "$expected"; then
    diff -u1 "$obtained" "$expected"
else
    echo "OK"
fi


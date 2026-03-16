#!/bin/bash

for i in $(seq 1 10); do
    key=$(openssl rand -hex 32)
    hash=$(printf "%s" "$key" | sha256sum | awk '{print $1}')
    echo "HashForServer: $hash CustomerKey: $key test$i"
done
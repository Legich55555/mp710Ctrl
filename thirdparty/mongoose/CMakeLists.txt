cmake_minimum_required(VERSION 2.8)

add_definitions(-DMG_DISABLE_MQTT -DMG_DISABLE_HTTP_DIGEST_AUTH -DMG_DISABLE_JSON_RPC -DMG_DISABLE_SHA1)
add_library(mongoose STATIC mongoose.c mongoose.h)

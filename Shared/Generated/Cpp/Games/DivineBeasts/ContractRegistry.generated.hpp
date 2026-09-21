// Code generated from Shared Contracts. DO NOT EDIT.
#pragma once
#include <array>
#include <string_view>
namespace DivineBeasts::Contracts {
inline constexpr std::array<std::string_view, 1> OpenAPIOperations = {"getDivineBeastsCatalog"};
struct OpenAPIRoute { std::string_view OperationId; std::string_view Method; std::string_view Path; };
inline constexpr std::array<OpenAPIRoute, 1> OpenAPIRoutes = {OpenAPIRoute{"getDivineBeastsCatalog", "GET", "/v1/games/divine-beasts/catalog"}};
inline constexpr std::array<std::string_view, 0> ProtoServices = {};
inline constexpr std::array<std::string_view, 0> ProtoRPCs = {};
}

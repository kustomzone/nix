#pragma once
///@file

#include "nix/util/compression-algo.hh"
#include "nix/util/types.hh"
#include "nix/util/hash.hh"
#include "nix/util/url.hh"
#include "nix/util/canon-path.hh"
#include "nix/store/path-info.hh"

#include <variant>

namespace nix {

struct StoreDirConfig;

/**
 * The URL field in a narinfo can be either:
 *
 * - A relative path within the binary cache (e.g., "nar/xxx.nar.xz").
 *
 *   This is the normal case which should be used.
 *
 * - An absolute URL pointing elsewhere (e.g., "https://example.com/nar/xxx.nar.xz")
 *
 *   This is an odd case that is somewhat an accidental feature. Only
 *   some types of `BinaryCacheStores` will support it, and only with
 *   restrictions.
 */
using NarUrl = std::variant<CanonPath, ParsedURL>;

/**
 * Parse a `.narinfo` URL string into a `NarUrl`.
 * If it looks like an absolute URL (contains "://"), parse as ParsedURL.
 * Otherwise, treat as a relative path within the binary cache.
 */
NarUrl parseNarUrl(std::string_view value);

/**
 * Convert a `NarUrl` to a string for serialization.
 */
std::string narUrlToString(const NarUrl & url);

/**
 * Check if a `NarUrl` is empty (root path or invalid).
 */
bool narUrlEmpty(const NarUrl & url);

struct UnkeyedNarInfo : virtual UnkeyedValidPathInfo
{
    NarUrl url;
    std::string compression; // FIXME: Use CompressionAlgo
    std::optional<Hash> fileHash;
    uint64_t fileSize = 0;

    UnkeyedNarInfo(UnkeyedValidPathInfo info)
        : UnkeyedValidPathInfo(std::move(info))
        , url{CanonPath::root}
    {
    }

    bool operator==(const UnkeyedNarInfo &) const = default;
    // TODO libc++ 16 (used by darwin) missing `std::optional::operator <=>`, can't do yet
    // auto operator <=>(const NarInfo &) const = default;

    nlohmann::json
    toJSON(const StoreDirConfig * store, bool includeImpureInfo, PathInfoJsonFormat format) const override;
    static UnkeyedNarInfo fromJSON(const StoreDirConfig * store, const nlohmann::json & json);
};

/**
 * Key and the extra NAR fields
 */
struct NarInfo : ValidPathInfo, UnkeyedNarInfo
{
    NarInfo() = delete;

    NarInfo(ValidPathInfo info)
        : UnkeyedValidPathInfo{static_cast<UnkeyedValidPathInfo &&>(info)}
        /* Later copies from `*this` are pointless. The argument is only
           there so the constructors can also call
           `UnkeyedValidPathInfo`, but this won't happen since the base
           class is virtual. Only this constructor (assuming it is most
           derived) will initialize that virtual base class. */
        , ValidPathInfo{info.path, static_cast<const UnkeyedValidPathInfo &>(*this)}
        , UnkeyedNarInfo{static_cast<const UnkeyedValidPathInfo &>(*this)}
    {
    }

    NarInfo(const StoreDirConfig & store, StorePath path, Hash narHash)
        : NarInfo{ValidPathInfo{std::move(path), UnkeyedValidPathInfo{store, narHash}}}
    {
    }

    NarInfo(std::string storeDir, StorePath path, Hash narHash)
        : NarInfo{ValidPathInfo{std::move(path), UnkeyedValidPathInfo{std::move(storeDir), narHash}}}
    {
    }

    static NarInfo
    makeFromCA(const StoreDirConfig & store, std::string_view name, ContentAddressWithReferences ca, Hash narHash)
    {
        return ValidPathInfo::makeFromCA(store, std::move(name), std::move(ca), narHash);
    }

    NarInfo(const StoreDirConfig & store, const std::string & s, const std::string & whence);

    bool operator==(const NarInfo &) const = default;

    std::string to_string(const StoreDirConfig & store) const;
};

} // namespace nix

JSON_IMPL(nix::UnkeyedNarInfo)

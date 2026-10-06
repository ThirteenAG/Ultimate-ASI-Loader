#include "internal.hpp"

namespace ual::vfs
{
    namespace
    {
        Resolved ResolveDepth(const std::wstring& key, int depth)
        {
            if (auto data = ApiFile(key))
            {
                Resolved r;
                r.kind = Resolved::Virtual;
                r.data = std::move(data);
                r.path = KeyToPath(key);
                r.time = r.data->written;
                return r;
            }
            std::wstring target;
            if (ApiPath(key, target))
            {
                // the target may itself be overloaded or redirected
                std::wstring targetKey;
                if (depth < 8 && MakeKey(target.c_str(), targetKey) && targetKey != key)
                {
                    Resolved again = ResolveDepth(targetKey, depth + 1);
                    if (again.kind != Resolved::None) return again;
                }
                Resolved r;
                r.kind = Resolved::Physical;
                r.path = target;
                return r;
            }
            return ResolveInLayers(key);
        }
    }

    Resolved Resolve(const std::wstring& key)
    {
        return ResolveDepth(key, 0);
    }

    bool OverlayListing(const std::wstring& dirKey, std::vector<ListedItem>& items)
    {
        bool any = ApiListing(dirKey, items);
        any |= LayerListing(dirKey, items);
        return any;
    }

    bool IsVirtualDirectory(const std::wstring& dirKey)
    {
        return ApiDirectory(dirKey) || ResolveInLayers(dirKey).kind == Resolved::Directory;
    }
}

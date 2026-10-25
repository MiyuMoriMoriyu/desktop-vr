#include "platform.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#include "openvr_driver.h"

namespace
{
using Clock = std::chrono::steady_clock;

class DirectMode final : public vr::IVRDriverDirectModeComponent
{
public:
    void CreateSwapTextureSet(uint32_t pid, const SwapTextureSetDesc_t* desc, SwapTextureSet_t* out) override
    {
        *out = {};
        vr::IVRIPCResourceManagerClient* resources = vr::VRIPCResourceManager();
        if (!resources)
        {
            return;
        }
        TextureSet set;
        set.pid = pid;
        for (vr::SharedTextureHandle_t& texture : set.textures)
        {
            if (!resources->NewSharedVulkanImage(desc->nFormat, desc->nWidth, desc->nHeight, true, false, false, 1, 1, 0, 0, &texture))
            {
                Release(set);
                return;
            }
        }
        std::copy(set.textures.begin(), set.textures.end(), out->rSharedTextureHandles);
        std::lock_guard<std::mutex> lock(lock_);
        sets_.push_back(set);
    }

    void DestroySwapTextureSet(vr::SharedTextureHandle_t texture) override
    {
        std::lock_guard<std::mutex> lock(lock_);
        RemoveIf([texture](const TextureSet& set) { return set.Contains(texture); });
    }

    void DestroyAllSwapTextureSets(uint32_t pid) override
    {
        std::lock_guard<std::mutex> lock(lock_);
        RemoveIf([pid](const TextureSet& set) { return set.pid == pid; });
    }

    void GetNextSwapTextureSetIndex(vr::SharedTextureHandle_t textures[2], uint32_t (*indices)[2]) override
    {
        std::lock_guard<std::mutex> lock(lock_);
        const TextureSet* previous = nullptr;
        for (int eye = 0; eye < 2; ++eye)
        {
            auto found = std::find_if(sets_.begin(), sets_.end(), [&](const TextureSet& set) { return set.Contains(textures[eye]); });
            TextureSet* set = found == sets_.end() ? nullptr : &*found;
            if (set && set != previous)
            {
                set->index = (set->index + 1) % static_cast<uint32_t>(set->textures.size());
            }
            (*indices)[eye] = set ? set->index : 0;
            previous = set;
        }
    }

    void PostPresent(const Throttling_t*) override
    {
        const auto period = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / platform::RefreshRate()));
        const auto now = Clock::now();
        nextVsync_ = nextVsync_ + period < now ? now + period : nextVsync_ + period;
        std::this_thread::sleep_until(nextVsync_);
        vr::VRServerDriverHost()->VsyncEvent(0.0);
    }

private:
    struct TextureSet
    {
        uint32_t pid = 0;
        std::array<vr::SharedTextureHandle_t, 3> textures{};
        uint32_t index = 0;

        bool Contains(vr::SharedTextureHandle_t texture) const
        {
            return texture != 0 && std::find(textures.begin(), textures.end(), texture) != textures.end();
        }
    };

    static void Release(const TextureSet& set)
    {
        for (vr::SharedTextureHandle_t texture : set.textures)
        {
            if (texture)
            {
                vr::VRIPCResourceManager()->UnrefResource(texture);
            }
        }
    }

    template <typename Predicate>
    void RemoveIf(Predicate predicate)
    {
        const auto end = std::remove_if(sets_.begin(), sets_.end(), [&](const TextureSet& set) {
            if (!predicate(set))
            {
                return false;
            }
            Release(set);
            return true;
        });
        sets_.erase(end, sets_.end());
    }

    std::mutex lock_;
    std::vector<TextureSet> sets_;
    Clock::time_point nextVsync_{};
};

DirectMode g_directMode;
}

vr::IVRDriverDirectModeComponent* platform::DirectMode()
{
    return &g_directMode;
}

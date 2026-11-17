#include "stdafx.h"

#include "doctest.h"

#include "TestModelSlots.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/AnimationTaskPool.h"
#include "Engine/Object/CharacterBonePose.h"
#include "Engine/Object/CharacterAnimationFade.h"
#include "Engine/AI/ZzzAI.h"
#include "Render/Models/AnimationSmoothing.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Scenes/SceneCore.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <span>

namespace
{
const std::filesystem::path PlayerFolder = std::filesystem::path(MU_TEST_DATA_DIR) / "Player";

void PlaceCharacter(OBJECT& o, float x, float scale, float bones[MAX_BONES][3][4])
{
    o.Type = MODEL_PLAYER;
    o.BoneTransform = bones;
    o.CurrentAction = 1;
    o.PriorAction = 0;
    o.AnimationFrame = 2.5f;
    o.PriorAnimationFrame = 1.f;
    o.Scale = scale;
    Vector(x, 300.f, 0.f, o.Position);
    Vector(0.f, 0.f, 45.f, o.Angle);
    Vector(0.f, 0.f, 0.f, o.HeadAngle);
}

constexpr int CrowdSize = 64;
using BoneSet = float[MAX_BONES][3][4];

// The first action from seed on that has keys to play.
unsigned short PlayableAction(const BMD& model, int seed)
{
    for (int step = 0; step < model.NumActions; ++step)
    {
        const int action = (seed + step) % model.NumActions;
        if (model.Actions[action].NumAnimationKeys > 0) return static_cast<unsigned short>(action);
    }
    return 0;
}

// A crowd of player models in every kind of pose: actions, blends between two actions, frames, turns, heads.
void PoseCrowd(CHARACTER* crowd, BoneSet* bones, const BMD& model)
{
    for (int i = 0; i < CrowdSize; ++i)
    {
        OBJECT& o = crowd[i].Object;
        PlaceCharacter(o, 100.f * static_cast<float>(i), 1.f, bones[i]);
        o.Live = true;
        o.Visible = true;
        o.CurrentAction = PlayableAction(model, i);
        o.PriorAction = PlayableAction(model, i * 7);
        const int keys = std::max<int>(1, model.Actions[o.CurrentAction].NumAnimationKeys);
        o.AnimationFrame = std::fmod(static_cast<float>(i) * 0.37f, static_cast<float>(keys));
        o.PriorAnimationFrame = static_cast<float>(i % 3);
        Vector(0.f, 0.f, static_cast<float>(i) * 11.f, o.Angle);
        Vector(static_cast<float>(i % 5) * 3.f, static_cast<float>(i % 3) * -4.f, 0.f, o.HeadAngle);
    }
}

void OpenPlayerModel(BMD& model)
{
    const std::wstring folder = PlayerFolder.wstring() + L"/";
    REQUIRE(model.Open2(folder.c_str(), L"player.bmd"));
    REQUIRE(model.NumBones > 0);
    REQUIRE(model.NumActions > 0);
}

// What render's own call builds for o: model space, the action taken from the model, as Calc_ObjectAnimation does.
void RenderBones(BMD& model, OBJECT& o, BoneSet& out)
{
    model.BodyHeight = 0.f;
    model.CurrentAction = o.CurrentAction;
    model.Animation(out, o.AnimationFrame, o.PriorAnimationFrame, o.PriorAction, o.Angle, o.HeadAngle, false, false);
}

// As RenderBones, with o's fade as AnimateForRender hands it over.
void RenderFadedBones(BMD& model, OBJECT& o, BoneSet& out)
{
    model.BodyHeight = 0.f;
    model.CurrentAction = o.CurrentAction;
    model.Animation(out, o.AnimationFrame, o.PriorAnimationFrame, o.PriorAction, o.Angle, o.HeadAngle, false, false,
                    nullptr, -1, MAX_BONES, Engine::Object::Fade::For(o, model.CurrentAction));
}

// The smoothed animation's state for one test: the setting, the client's characters, the scene and the clock,
// all put back afterwards.
class SmoothingScope
{
public:
    SmoothingScope(bool enabled, CHARACTER* characters)
        : m_characters(CharactersClient), m_scene(SceneFlag), m_worldTime(WorldTime), m_frameFactor(FPS_ANIMATION_FACTOR)
    {
        // One animation step per PlayAnimation() at its speed, as at the reference frame rate.
        FPS_ANIMATION_FACTOR = 1.f;
        Render::AnimationSmoothing::SetForTests(enabled, Render::AnimationSmoothing::kDefaultFadeMs);
        CharactersClient = characters;
        SceneFlag = MAIN_SCENE;
        WorldTime = 1000.0;
    }

    ~SmoothingScope()
    {
        Render::AnimationSmoothing::SetForTests(false, Render::AnimationSmoothing::kDefaultFadeMs);
        CharactersClient = m_characters;
        SceneFlag = m_scene;
        WorldTime = m_worldTime;
        FPS_ANIMATION_FACTOR = m_frameFactor;
    }

    SmoothingScope(const SmoothingScope&) = delete;
    SmoothingScope& operator=(const SmoothingScope&) = delete;

private:
    CHARACTER* m_characters;
    EGameScene m_scene;
    double m_worldTime;
    float m_frameFactor;
};

// Half of the crowd in the middle of a fade from another action, each at its own share.
void FadeCrowd(CHARACTER* crowd, const BMD& model)
{
    for (int i = 0; i < CrowdSize; i += 2)
    {
        OBJECT& o = crowd[i].Object;
        Render::AnimationSmoothing::AnimationFade& fade = o.Fade;
        fade.active = true;
        fade.type = o.Type;
        fade.toAction = o.CurrentAction;
        const unsigned short from = PlayableAction(model, i * 3 + 1);
        const int keys = std::max<int>(1, model.Actions[from].NumAnimationKeys);
        fade.from = {from, from, static_cast<float>(i % keys), std::fmod(static_cast<float>(i) * 0.53f, static_cast<float>(keys))};
        fade.startMs = 0.0;
        fade.weight = static_cast<float>(i + 1) / static_cast<float>(CrowdSize + 1);
    }
}

bool AllFinite(const BoneSet& bones, int count)
{
    const float* values = &bones[0][0][0];
    for (int i = 0; i < count * 12; ++i)
    {
        if (!std::isfinite(values[i])) return false;
    }
    return true;
}
} // namespace

TEST_CASE("Character bones from the pre-pass do not depend on the model's last drawn object [data][animation]")
{
    TestModelSlots slots;
    BMD& model = Models[MODEL_PLAYER];
    const std::wstring folder = PlayerFolder.wstring() + L"/";
    REQUIRE(model.Open2(folder.c_str(), L"player.bmd"));
    REQUIRE(model.NumBones > 0);

    static float nearBones[MAX_BONES][3][4];
    static float farBones[MAX_BONES][3][4];
    static float alone[MAX_BONES][3][4];
    OBJECT a;
    OBJECT b;
    PlaceCharacter(a, 100.f, 0.9f, nearBones);
    PlaceCharacter(b, 9000.f, 1.4f, farBones);

    // Another object of the same model was drawn last somewhere else.
    Vector(5000.f, 5000.f, 500.f, model.BodyOrigin);
    model.BodyScale = 3.f;
    AnimateCharacterBones(&b);
    AnimateCharacterBones(&a);

    // Render's own bones for a: model space, after setting a's origin and scale.
    VectorCopy(a.Position, model.BodyOrigin);
    model.BodyScale = a.Scale;
    model.Animation(alone, a.AnimationFrame, a.PriorAnimationFrame, a.PriorAction, a.Angle, a.HeadAngle, false, false,
                    nullptr, a.CurrentAction);

    CHECK(a.EnableBoneMatrix);
    CHECK(std::memcmp(nearBones, alone, sizeof(float) * 12 * model.NumBones) == 0);
    // The root stays near the model origin rather than at a world position.
    CHECK(std::abs(nearBones[0][0][3]) < 1000.f);
    CHECK(std::abs(farBones[0][0][3]) < 1000.f);
}

TEST_CASE("The pre-pass animates characters at render's height, not one an item preview left [data][animation]")
{
    TestModelSlots slots;
    BMD& model = Models[MODEL_PLAYER];
    const std::wstring folder = PlayerFolder.wstring() + L"/";
    REQUIRE(model.Open2(folder.c_str(), L"player.bmd"));

    // The root height applies only to actions that lock positions.
    int lockedAction = -1;
    for (int action = 0; action < model.NumActions && lockedAction < 0; ++action)
    {
        if (model.Actions[action].LockPositions && model.Actions[action].NumAnimationKeys > 1) lockedAction = action;
    }
    REQUIRE(lockedAction >= 0);

    static float bones[MAX_BONES][3][4];
    static float rendered[MAX_BONES][3][4];
    static CHARACTER characters[1];
    OBJECT& o = characters[0].Object;
    PlaceCharacter(o, 100.f, 1.f, bones);
    o.CurrentAction = o.PriorAction = static_cast<unsigned short>(lockedAction);
    o.Live = true;
    o.Visible = true;

    model.BodyHeight = -160.f;
    UpdateCharactersAnimationParallel(std::span<CHARACTER>(characters, 1));

    model.BodyHeight = 0.f;
    model.Animation(rendered, o.AnimationFrame, o.PriorAnimationFrame, o.PriorAction, o.Angle, o.HeadAngle, false, false,
                    nullptr, o.CurrentAction);
    CHECK(bones[0][2][3] == rendered[0][2][3]);
    CHECK(std::memcmp(bones, rendered, sizeof(float) * 12 * model.NumBones) == 0);
}

#ifdef MU_ENABLE_CONTROL_SOCKET
TEST_CASE("The bones check counts a character whose bone differs [data][animation]")
{
    static float prepass[2][3][4];
    static float rendered[2][3][4];
    g_PrepassBoneCheck = PrepassBoneCheck{};
    RecordPrepassBones(prepass, rendered, 2, true);
    rendered[1][2][3] = -160.f;
    RecordPrepassBones(prepass, rendered, 2, false);

    CHECK(g_PrepassBoneCheck.checked == 2);
    CHECK(g_PrepassBoneCheck.players == 1);
    CHECK(g_PrepassBoneCheck.mismatched == 1);
    CHECK(g_PrepassBoneCheck.maxDelta == doctest::Approx(160.f));
    g_PrepassBoneCheck = PrepassBoneCheck{};
}
#endif

TEST_CASE("Bones from the animation workers equal the main thread's for a varied crowd [data][animation]")
{
    TestModelSlots slots;
    BMD& model = Models[MODEL_PLAYER];
    OpenPlayerModel(model);

    static CHARACTER crowd[CrowdSize];
    static BoneSet serial[CrowdSize];
    static BoneSet pooled[CrowdSize];
    const std::size_t previousWorkers = g_AnimationTaskPoolWorkers;

    PoseCrowd(crowd, serial, model);
    g_AnimationTaskPoolWorkers = 0;
    UpdateCharactersAnimationParallel(std::span<CHARACTER>(crowd, CrowdSize));
    CHECK_FALSE(g_LastAnimationWasParallel);

    for (std::size_t workers = 1; workers <= 4; ++workers)
    {
        PoseCrowd(crowd, pooled, model);
        std::memset(pooled, 0, sizeof(pooled));
        g_AnimationTaskPoolWorkers = workers;
        UpdateCharactersAnimationParallel(std::span<CHARACTER>(crowd, CrowdSize));
        CHECK(g_LastAnimationWasParallel);
        WaitCharactersAnimation();
        CHECK_FALSE(AnimationTaskPool::Instance().InFlight());
        for (int i = 0; i < CrowdSize; ++i)
        {
            CHECK(crowd[i].Object.EnableBoneMatrix);
            CHECK(std::memcmp(serial[i], pooled[i], sizeof(BoneSet)) == 0);
        }
        AnimationTaskPool::Instance().Shutdown();
    }
    g_AnimationTaskPoolWorkers = previousWorkers;
}

TEST_CASE("With workers set but too few characters, or none set, the main thread still builds the bones [data][animation]")
{
    TestModelSlots slots;
    BMD& model = Models[MODEL_PLAYER];
    OpenPlayerModel(model);

    static CHARACTER crowd[CrowdSize];
    static BoneSet bones[CrowdSize];
    static BoneSet rendered;
    const std::size_t previousWorkers = g_AnimationTaskPoolWorkers;

    // Five characters: below the threshold the workers are not even started.
    PoseCrowd(crowd, bones, model);
    std::memset(bones, 0, sizeof(bones));
    g_AnimationTaskPoolWorkers = 3;
    UpdateCharactersAnimationParallel(std::span<CHARACTER>(crowd, 5));
    CHECK_FALSE(g_LastAnimationWasParallel);
    CHECK_FALSE(AnimationTaskPool::Instance().IsRunning());

    // The whole crowd with the workers off.
    g_AnimationTaskPoolWorkers = 0;
    UpdateCharactersAnimationParallel(std::span<CHARACTER>(crowd, CrowdSize));
    CHECK_FALSE(g_LastAnimationWasParallel);
    WaitCharactersAnimation();
    for (int i = 0; i < CrowdSize; ++i)
    {
        OBJECT& o = crowd[i].Object;
        CHECK(o.EnableBoneMatrix);
        RenderBones(model, o, rendered);
        CHECK(std::memcmp(bones[i], rendered, sizeof(float) * 12 * model.NumBones) == 0);
    }
    g_AnimationTaskPoolWorkers = previousWorkers;
}

TEST_CASE("Render may keep the pre-pass bones only while every input is the same [data][animation]")
{
    TestModelSlots slots;
    BMD& model = Models[MODEL_PLAYER];
    OpenPlayerModel(model);
    namespace BonePose = Engine::Object::BonePose;

    static CHARACTER crowd[CrowdSize];
    static BoneSet bones[CrowdSize];
    static BoneSet rendered;
    PoseCrowd(crowd, bones, model);
    UpdateCharactersAnimationParallel(std::span<CHARACTER>(crowd, CrowdSize));
    WaitCharactersAnimation();

    // Wherever render's call has the pre-pass's inputs, the bones it would build are the same bits.
    for (int i = 0; i < CrowdSize; ++i)
    {
        OBJECT& o = crowd[i].Object;
        REQUIRE(BonePose::Matches(o, 0.f));
        RenderBones(model, o, rendered);
        CHECK(std::memcmp(bones[i], rendered, sizeof(BoneSet)) == 0);
    }

    OBJECT& o = crowd[5].Object;
    const float frame = o.AnimationFrame;
    const float priorFrame = o.PriorAnimationFrame;
    const unsigned short action = o.CurrentAction;
    const unsigned short priorAction = o.PriorAction;
    const int type = o.Type;
    vec3_t angle;
    vec3_t headAngle;
    VectorCopy(o.Angle, angle);
    VectorCopy(o.HeadAngle, headAngle);
    auto restore = [&]() {
        o.AnimationFrame = frame;
        o.PriorAnimationFrame = priorFrame;
        o.CurrentAction = action;
        o.PriorAction = priorAction;
        o.Type = type;
        VectorCopy(angle, o.Angle);
        VectorCopy(headAngle, o.HeadAngle);
        REQUIRE(BonePose::Matches(o, 0.f));
    };

    // A stunned or sleeping owner holds the frame at 0 in render.
    REQUIRE(frame != 0.f);
    o.AnimationFrame = 0.f;
    CHECK_FALSE(BonePose::Matches(o, 0.f));
    restore();
    // An item preview left another height on the model.
    CHECK_FALSE(BonePose::Matches(o, -160.f));
    o.CurrentAction = static_cast<unsigned short>((o.CurrentAction + 1) % model.NumActions);
    CHECK_FALSE(BonePose::Matches(o, 0.f));
    restore();
    o.PriorAction = static_cast<unsigned short>((o.PriorAction + 1) % model.NumActions);
    CHECK_FALSE(BonePose::Matches(o, 0.f));
    restore();
    o.PriorAnimationFrame += 0.25f;
    CHECK_FALSE(BonePose::Matches(o, 0.f));
    restore();
    o.Angle[2] = std::nextafter(o.Angle[2], 1000.f);
    CHECK_FALSE(BonePose::Matches(o, 0.f));
    restore();
    o.HeadAngle[0] += 1.f;
    CHECK_FALSE(BonePose::Matches(o, 0.f));
    restore();
    // Another model drawn through the same object (o->Type++ for a second body).
    ++o.Type;
    CHECK_FALSE(BonePose::Matches(o, 0.f));
    restore();

    // Anything else wrote the bones.
    BonePose::Forget(o);
    CHECK_FALSE(BonePose::Matches(o, 0.f));

    // A model loaded since (an HD swap reloads models): none of the bones are kept.
    OBJECT& other = crowd[6].Object;
    REQUIRE(BonePose::Matches(other, 0.f));
    const std::wstring folder = PlayerFolder.wstring() + L"/";
    REQUIRE(Models[MODEL_PLAYER + 1].Open2(folder.c_str(), L"player.bmd"));
    CHECK_FALSE(BonePose::Matches(other, 0.f));

    // The next pre-pass forgets what it does not build again.
    crowd[7].Object.Visible = false;
    UpdateCharactersAnimationParallel(std::span<CHARACTER>(crowd, CrowdSize));
    WaitCharactersAnimation();
    CHECK(BonePose::Matches(other, 0.f));
    CHECK_FALSE(BonePose::Matches(crowd[7].Object, 0.f));
}

TEST_CASE("With the smoothed animation off the bones stay bit for bit as before [data][animation][anim]")
{
    TestModelSlots slots;
    BMD& model = Models[MODEL_PLAYER];
    OpenPlayerModel(model);

    static CHARACTER crowd[CrowdSize];
    static BoneSet unused[CrowdSize];
    static BoneSet legacy[CrowdSize];
    static BoneSet rendered;
    PoseCrowd(crowd, unused, model);
    SmoothingScope scope(false, crowd);

    model.SmoothKeys = false;
    for (int i = 0; i < CrowdSize; ++i)
    {
        RenderBones(model, crowd[i].Object, legacy[i]);
    }

    // A smoothed model with the setting off, even with fades left on the objects.
    FadeCrowd(crowd, model);
    model.SmoothKeys = true;
    for (int i = 0; i < CrowdSize; ++i)
    {
        OBJECT& o = crowd[i].Object;
        model.BodyHeight = 0.f;
        model.CurrentAction = o.CurrentAction;
        model.Animation(rendered, o.AnimationFrame, o.PriorAnimationFrame, o.PriorAction, o.Angle, o.HeadAngle, false,
                        false, nullptr, -1, MAX_BONES, &o.Fade);
        CHECK(std::memcmp(legacy[i], rendered, sizeof(BoneSet)) == 0);
    }

    // The setting on, but a model that is not smoothed (map objects, items).
    Render::AnimationSmoothing::SetForTests(true, Render::AnimationSmoothing::kDefaultFadeMs);
    model.SmoothKeys = false;
    for (int i = 0; i < CrowdSize; ++i)
    {
        OBJECT& o = crowd[i].Object;
        model.BodyHeight = 0.f;
        model.CurrentAction = o.CurrentAction;
        model.Animation(rendered, o.AnimationFrame, o.PriorAnimationFrame, o.PriorAction, o.Angle, o.HeadAngle, false,
                        false, nullptr, -1, MAX_BONES, &o.Fade);
        CHECK(std::memcmp(legacy[i], rendered, sizeof(BoneSet)) == 0);
    }

    // On for a smoothed model: the curves and fades change poses, never into something that is not a number.
    model.SmoothKeys = true;
    int changed = 0;
    for (int i = 0; i < CrowdSize; ++i)
    {
        RenderFadedBones(model, crowd[i].Object, rendered);
        CHECK(AllFinite(rendered, model.NumBones));
        if (std::memcmp(legacy[i], rendered, sizeof(float) * 12 * model.NumBones) != 0) ++changed;
    }
    CHECK(changed >= CrowdSize / 4);

    // Two neighbouring keys of one action without a fade: the curve alone moves the pose off the straight line.
    int longAction = -1;
    for (int action = 0; action < model.NumActions && longAction < 0; ++action)
    {
        if (model.Actions[action].NumAnimationKeys >= 6) longAction = action;
    }
    REQUIRE(longAction >= 0);
    OBJECT& curved = crowd[1].Object;
    curved.Fade = {};
    curved.CurrentAction = curved.PriorAction = static_cast<unsigned short>(longAction);
    curved.PriorAnimationFrame = 2.f;
    curved.AnimationFrame = 3.4f;
    static BoneSet straight;
    model.SmoothKeys = false;
    RenderBones(model, curved, straight);
    model.SmoothKeys = true;
    RenderFadedBones(model, curved, rendered);
    CHECK(std::memcmp(straight, rendered, sizeof(float) * 12 * model.NumBones) != 0);
    // At a key itself the curve and the line meet.
    curved.AnimationFrame = 3.f;
    model.SmoothKeys = false;
    RenderBones(model, curved, straight);
    model.SmoothKeys = true;
    RenderFadedBones(model, curved, rendered);
    for (int bone = 0; bone < model.NumBones; ++bone)
    {
        for (int row = 0; row < 3; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                CHECK(rendered[bone][row][column] == doctest::Approx(straight[bone][row][column]).epsilon(1e-4));
            }
        }
    }
}

TEST_CASE("Smoothed bones from the animation workers equal the main thread's and render's, fades included [data][animation][anim]")
{
    TestModelSlots slots;
    BMD& model = Models[MODEL_PLAYER];
    OpenPlayerModel(model);
    model.SmoothKeys = true;

    static CHARACTER crowd[CrowdSize];
    static BoneSet serial[CrowdSize];
    static BoneSet pooled[CrowdSize];
    static BoneSet rendered;
    SmoothingScope scope(true, crowd);
    const std::size_t previousWorkers = g_AnimationTaskPoolWorkers;

    PoseCrowd(crowd, serial, model);
    FadeCrowd(crowd, model);
    g_AnimationTaskPoolWorkers = 0;
    UpdateCharactersAnimationParallel(std::span<CHARACTER>(crowd, CrowdSize));
    CHECK_FALSE(g_LastAnimationWasParallel);

    for (std::size_t workers = 1; workers <= 4; ++workers)
    {
        PoseCrowd(crowd, pooled, model);
        FadeCrowd(crowd, model);
        std::memset(pooled, 0, sizeof(pooled));
        g_AnimationTaskPoolWorkers = workers;
        UpdateCharactersAnimationParallel(std::span<CHARACTER>(crowd, CrowdSize));
        CHECK(g_LastAnimationWasParallel);
        WaitCharactersAnimation();
        for (int i = 0; i < CrowdSize; ++i)
        {
            CHECK(std::memcmp(serial[i], pooled[i], sizeof(BoneSet)) == 0);
        }
        AnimationTaskPool::Instance().Shutdown();
    }
    g_AnimationTaskPoolWorkers = previousWorkers;

    for (int i = 0; i < CrowdSize; ++i)
    {
        OBJECT& o = crowd[i].Object;
        REQUIRE(Engine::Object::BonePose::Matches(o, 0.f));
        RenderFadedBones(model, o, rendered);
        CHECK(std::memcmp(pooled[i], rendered, sizeof(BoneSet)) == 0);
        CHECK(AllFinite(rendered, model.NumBones));
    }
}

TEST_CASE("Render keeps the pre-pass bones only while the fade and the setting are the same [data][animation][anim]")
{
    TestModelSlots slots;
    BMD& model = Models[MODEL_PLAYER];
    OpenPlayerModel(model);
    model.SmoothKeys = true;
    namespace BonePose = Engine::Object::BonePose;

    static CHARACTER crowd[CrowdSize];
    static BoneSet bones[CrowdSize];
    SmoothingScope scope(true, crowd);
    PoseCrowd(crowd, bones, model);
    FadeCrowd(crowd, model);
    UpdateCharactersAnimationParallel(std::span<CHARACTER>(crowd, CrowdSize));
    WaitCharactersAnimation();

    OBJECT& o = crowd[4].Object;
    REQUIRE(o.Fade.active);
    REQUIRE(BonePose::Matches(o, 0.f));
    const Render::AnimationSmoothing::AnimationFade fade = o.Fade;
    auto restore = [&]() {
        o.Fade = fade;
        Render::AnimationSmoothing::SetForTests(true, Render::AnimationSmoothing::kDefaultFadeMs);
        REQUIRE(BonePose::Matches(o, 0.f));
    };

    o.Fade.weight = std::nextafter(o.Fade.weight, 1.f);
    CHECK_FALSE(BonePose::Matches(o, 0.f));
    restore();
    o.Fade.from.frame += 0.25f;
    CHECK_FALSE(BonePose::Matches(o, 0.f));
    restore();
    o.Fade.from.priorFrame += 1.f;
    CHECK_FALSE(BonePose::Matches(o, 0.f));
    restore();
    o.Fade.from.priorAction = static_cast<unsigned short>((o.Fade.from.priorAction + 1) % model.NumActions);
    CHECK_FALSE(BonePose::Matches(o, 0.f));
    restore();
    o.Fade.active = false;
    CHECK_FALSE(BonePose::Matches(o, 0.f));
    restore();
    Render::AnimationSmoothing::SetForTests(false, Render::AnimationSmoothing::kDefaultFadeMs);
    CHECK_FALSE(BonePose::Matches(o, 0.f));
    restore();

    // Without a fade the setting still counts: the curves alone change the bones.
    OBJECT& still = crowd[5].Object;
    REQUIRE_FALSE(still.Fade.active);
    REQUIRE(BonePose::Matches(still, 0.f));
    Render::AnimationSmoothing::SetForTests(false, Render::AnimationSmoothing::kDefaultFadeMs);
    CHECK_FALSE(BonePose::Matches(still, 0.f));
    Render::AnimationSmoothing::SetForTests(true, Render::AnimationSmoothing::kDefaultFadeMs);
    CHECK(BonePose::Matches(still, 0.f));
}

TEST_CASE("The smoothed animation never changes the frames the game reads [data][animation][anim]")
{
    TestModelSlots slots;
    BMD& model = Models[MODEL_PLAYER];
    OpenPlayerModel(model);
    model.SmoothKeys = true;

    struct Frames
    {
        unsigned short currentAction;
        unsigned short priorAction;
        float animationFrame;
        float priorAnimationFrame;
    };
    constexpr int Steps = 240;
    static Frames recorded[2][Steps];
    static CHARACTER characters[2][1];
    static BoneSet bones[2];
    bool faded = false;
    bool fadedOff = false;

    for (int run = 0; run < 2; ++run)
    {
        const bool smooth = run == 1;
        SmoothingScope scope(smooth, characters[run]);
        OBJECT& o = characters[run][0].Object;
        PlaceCharacter(o, 100.f, 1.f, bones[run]);
        o.Live = true;
        o.Visible = true;
        o.Fade = {};
        o.CurrentAction = PlayableAction(model, 1);
        o.PriorAction = o.CurrentAction;
        o.AnimationFrame = 0.f;
        o.PriorAnimationFrame = 0.f;
        for (int step = 0; step < Steps; ++step)
        {
            if (step % 9 == 0)
            {
                // Often enough that a fade is cut short by the next action; every third one without blending.
                SetAction(&o, PlayableAction(model, step / 9 * 5 + 2), (step / 9) % 3 != 2);
            }
            model.CurrentAction = o.CurrentAction;
            const bool played = model.PlayAnimation(&o.AnimationFrame, &o.PriorAnimationFrame, &o.PriorAction, 0.31f,
                                                    o.Position, o.Angle);
            Engine::Object::Fade::Tick(o, played);
            if (o.Fade.active)
            {
                (smooth ? faded : fadedOff) = true;
                CHECK(o.Fade.weight >= 0.f);
                CHECK(o.Fade.weight <= 1.f);
            }
            recorded[run][step] = {o.CurrentAction, o.PriorAction, o.AnimationFrame, o.PriorAnimationFrame};
            WorldTime += 25.0;
        }
    }

    CHECK(faded);
    CHECK_FALSE(fadedOff);
    for (int step = 0; step < Steps; ++step)
    {
        CHECK(std::memcmp(&recorded[0][step], &recorded[1][step], sizeof(Frames)) == 0);
    }
}

TEST_CASE("A fade cut short keeps its old pose in its first half and starts from the new action after [data][animation][anim]")
{
    TestModelSlots slots;
    BMD& model = Models[MODEL_PLAYER];
    OpenPlayerModel(model);
    model.SmoothKeys = true;

    static CHARACTER characters[1];
    static BoneSet bones;
    SmoothingScope scope(true, characters);
    OBJECT& o = characters[0].Object;
    PlaceCharacter(o, 100.f, 1.f, bones);
    o.Live = true;
    o.Visible = true;
    o.Fade = {};

    const unsigned short first = PlayableAction(model, 1);
    const unsigned short second = PlayableAction(model, first + 1);
    const unsigned short third = PlayableAction(model, second + 1);
    REQUIRE(first != second);
    REQUIRE(second != third);
    REQUIRE(third != first);
    REQUIRE(model.Actions[second].NumAnimationKeys >= 3);
    REQUIRE(model.Actions[third].NumAnimationKeys >= 3);
    o.CurrentAction = o.PriorAction = first;
    o.AnimationFrame = 1.5f;
    o.PriorAnimationFrame = 1.f;

    SetAction(&o, second);
    REQUIRE(o.Fade.active);
    CHECK(o.Fade.toAction == second);
    CHECK(o.Fade.from.action == first);
    CHECK(o.Fade.from.frame == 1.5f);

    // Cut short early: still from the first action's pose.
    o.Fade.weight = 0.3f;
    o.AnimationFrame = 0.4f;
    SetAction(&o, third);
    REQUIRE(o.Fade.active);
    CHECK(o.Fade.toAction == third);
    CHECK(o.Fade.from.action == first);

    // Cut short late: from the second action's (stretched) start.
    o.Fade.weight = 0.8f;
    o.AnimationFrame = 1.0f;
    SetAction(&o, second);
    REQUIRE(o.Fade.active);
    CHECK(o.Fade.from.action == third);
    CHECK(o.Fade.from.priorAction == third);
    CHECK(o.Fade.from.frame == doctest::Approx(1.5f));

    // Without blending, or outside the main scene, actions switch as before.
    SetAction(&o, first, false);
    CHECK_FALSE(o.Fade.active);
    SceneFlag = CHARACTER_SCENE;
    SetAction(&o, second);
    CHECK_FALSE(o.Fade.active);
    SceneFlag = MAIN_SCENE;

    // An object that is none of the client's characters never fades.
    static OBJECT preview;
    static BoneSet previewBones;
    PlaceCharacter(preview, 0.f, 1.f, previewBones);
    preview.Visible = true;
    preview.CurrentAction = first;
    SetAction(&preview, second);
    CHECK_FALSE(preview.Fade.active);
}

#pragma once

namespace Engine::Object
{
    // Keeps the hero's attack rate independent of the frame rate.
    //
    // A swing animation ends somewhere inside a frame. The next swing used to start at key 0 on the
    // following frame, so the time the last swing ran past its end was lost and every swing lasted a whole
    // number of frames: at 60 fps an attack speed of 2000 gave 20 swings a second where the animation allows
    // about 29, while an uncapped client got 29. Each swing sends its hit when it starts, so the frame rate
    // decided the damage. SwingCadence carries that time into the next swing when it starts right away.
    class SwingCadence
    {
    public:
        // A swing ended on `frame`, `overshootKeys` animation keys past its end while playing at `playSpeed`
        // keys per reference frame; `frameFactor` is the length of this frame in reference frames.
        void RecordSwingEnd(float overshootKeys, float playSpeed, float frameFactor, unsigned int frame);

        // The key a swing starting on `frame` at `playSpeed` begins at: the carried time when the last swing
        // ended on this frame or the one before, otherwise 0. The carry is used up either way.
        float TakeStartKey(float playSpeed, unsigned int frame);

    private:
        // A swing that starts later than this many frames after the last one ended didn't follow it right
        // away (the player paused, walked or clicked again), so nothing is carried.
        static constexpr unsigned int MaxFramesBetweenSwings = 1;

        float m_carryReferenceFrames = 0.f;
        unsigned int m_endFrame = 0;
        bool m_hasCarry = false;
    };

    SwingCadence& HeroSwingCadence();
}

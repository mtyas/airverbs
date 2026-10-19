#pragma once

#include <juce_opengl/juce_opengl.h>
#include "Widgets.h"
#include <random>

namespace airverbs
{

/**
    The 3D observatory. The floor is the vector pad: drag the listener puck to blend the
    four stars, right-drag to orbit the camera, scroll to zoom, double-click to recentre.
    Beams show the active topology with photons carrying signal between stars; particles
    are emitted in proportion to each star's live output energy and hang in the air when
    the reverb is frozen.
*/
class ConstellationView : public juce::Component,
                          private juce::OpenGLRenderer,
                          private juce::Timer
{
public:
    explicit ConstellationView (AirverbsProcessor&);
    ~ConstellationView() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    struct V3 { float x = 0, y = 0, z = 0; };

    /** Dev aid: grabs the next GL frame into an image (see AIRVERBS_SNAPSHOT in the editor). */
    void requestCapture() { captureRequested = true; }
    juce::Image takeCapture() { const juce::SpinLock::ScopedLockType sl (camLock); return std::exchange (captured, {}); }

private:
    // OpenGLRenderer
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;
    void timerCallback() override { repaint(); }

    struct Mat4 { float m[16]; };
    struct Camera { V3 eye, target, right, up, forward; float fovY = 0.7f, aspect = 1.0f; Mat4 viewProj; };
    struct Particle { V3 pos, vel; float life = 0, maxLife = 1, size = 0.05f; int star = 0; float seed = 0; };
    struct SpriteVertex { float x, y, z, r, g, b, a, size; };
    struct RibbonVertex { float x, y, z, u, v, r, g, b, a; };

    Camera computeCamera (float time, float aspect) const;
    bool screenToFloor (juce::Point<float> p, float& fx, float& fz) const;
    juce::Point<float> project (const V3&) const;
    void setVector (float fx, float fz);
    void buildScene (float dt);
    void addRibbon (const V3& a, const V3& b, float width, juce::Colour c, float intensity, float uOffset = 0.0f);
    void addSprite (const V3& p, float size, juce::Colour c, float alpha);
    V3 starPos (int k) const;
    V3 corePos() const { return { 0.0f, 0.78f, 0.0f }; }

    AirverbsProcessor& processor;
    juce::OpenGLContext context;

    // GL objects (GL thread only)
    std::unique_ptr<juce::OpenGLShaderProgram> skyProg, floorProg, spriteProg, ribbonProg;
    juce::uint32 vao = 0, quadVbo = 0, floorVbo = 0, spriteVbo = 0, ribbonVbo = 0;
    bool glReady = false;

    // scene (GL thread only)
    std::vector<Particle> particles;
    std::vector<SpriteVertex> sprites;
    std::vector<RibbonVertex> ribbons;
    std::vector<V3> trail;
    std::array<float, 4> emitAcc {};
    std::minstd_rand rng { 1234 };
    double lastTime = 0.0;
    float animTime = 0.0f;
    Camera cam;

    // shared between threads
    std::atomic<float> yaw { 0.0f }, pitch { 0.62f }, distance { 4.1f };
    mutable juce::SpinLock camLock;
    Camera sharedCam;
    float sharedTime = 0.0f;

    std::atomic<bool> captureRequested { false };
    juce::Image captured;

    // interaction
    enum class Drag { None, Puck, Camera } drag = Drag::None;
    juce::Point<float> lastMouse;
    float dragYaw = 0, dragPitch = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConstellationView)
};

} // namespace airverbs

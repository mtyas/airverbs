#include "ConstellationView.h"
#include "../PluginProcessor.h"

using namespace juce::gl;

namespace airverbs
{

namespace
{
    using V3 = ConstellationView::V3;
    inline V3 operator+ (V3 a, V3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
    inline V3 operator- (V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
    inline V3 operator* (V3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
    inline float dot (V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    inline V3 cross (V3 a, V3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
    inline float len (V3 a) { return std::sqrt (dot (a, a)); }
    inline V3 norm (V3 a) { const float l = len (a); return l > 1.0e-6f ? a * (1.0f / l) : V3 { 0, 1, 0 }; }
    inline V3 lerp (V3 a, V3 b, float t) { return a + (b - a) * t; }

    const char* skyVS = R"(#version 150
in vec2 aPos; out vec2 vUv;
void main() { vUv = aPos * 0.5 + 0.5; gl_Position = vec4 (aPos, 0.0, 1.0); })";

    const char* skyFS = R"(#version 150
in vec2 vUv; out vec4 fragColor;
uniform float uTime; uniform float uFreeze; uniform vec3 uTint; uniform float uScale;
float hash (vec2 p) { return fract (sin (dot (p, vec2 (127.1, 311.7))) * 43758.5453); }
void main()
{
    vec2 p = vUv;
    vec3 col = mix (vec3 (0.006, 0.007, 0.012), vec3 (0.020, 0.026, 0.050), pow (p.y, 1.3));
    float n = sin (p.x * 3.1 + uTime * 0.03) * sin (p.y * 4.7 - uTime * 0.021) + 0.5 * sin (p.x * 7.3 - p.y * 5.1 + uTime * 0.05);
    col += uTint * 0.05 * clamp (0.5 + 0.5 * n, 0.0, 1.0) * p.y;
    vec2 g = floor (gl_FragCoord.xy / (2.5 * uScale));
    float h = hash (g);
    if (h > 0.9965)
    {
        float tw = 0.45 + 0.55 * sin (uTime * (0.7 + h * 4.0) + h * 50.0);
        col += vec3 (0.65, 0.75, 1.0) * tw * (h - 0.9965) * 220.0 * smoothstep (0.25, 1.0, p.y);
    }
    col += vec3 (0.04, 0.07, 0.10) * uFreeze * p.y;
    fragColor = vec4 (col, 1.0);
})";

    const char* floorVS = R"(#version 150
in vec3 aPos; out vec3 vPos; uniform mat4 uViewProj;
void main() { vPos = aPos; gl_Position = uViewProj * vec4 (aPos, 1.0); })";

    const char* floorFS = R"(#version 150
in vec3 vPos; out vec4 fragColor;
uniform vec3 uStarPos[4]; uniform vec3 uStarCol[4]; uniform float uStarE[4]; uniform float uStarW[4];
uniform vec2 uPuck; uniform float uTime; uniform float uFreeze;
float gridLine (float c, float spacing)
{
    float d = abs (fract (c / spacing - 0.5) - 0.5) * spacing;
    float w = fwidth (c) * 1.1;
    return 1.0 - smoothstep (0.0, w, d);
}
void main()
{
    vec2 p = vPos.xz;
    float edge = max (abs (p.x), abs (p.y));
    float inside = 1.0 - smoothstep (1.0, 1.015, edge);
    vec3 col = vec3 (0.0);
    float g = max (gridLine (p.x, 0.25), gridLine (p.y, 0.25));
    float fine = max (gridLine (p.x, 0.0625), gridLine (p.y, 0.0625));
    col += vec3 (0.18, 0.28, 0.40) * (g * 0.32 + fine * 0.06) * inside;
    float border = 1.0 - smoothstep (0.0, fwidth (edge) * 1.5, abs (edge - 1.0));
    col += vec3 (0.45, 0.75, 0.95) * border * 0.55;
    float speed = 1.0 - uFreeze;
    for (int i = 0; i < 4; i++)
    {
        float d = length (p - uStarPos[i].xz);
        col += uStarCol[i] * (0.05 + uStarW[i] * 0.22 + uStarE[i] * 2.2) * exp (-d * d * 4.0);
        float rip = 0.5 + 0.5 * sin (d * 26.0 - uTime * 3.5 * speed - float (i));
        col += uStarCol[i] * rip * exp (-d * 2.2) * min (uStarE[i] * 3.0, 1.0) * 0.35 * inside;
    }
    float dp = length (p - uPuck);
    float ring = 1.0 - smoothstep (0.0, fwidth (dp) * 1.5, abs (dp - 0.085 - 0.012 * sin (uTime * 2.7)));
    col += vec3 (0.85, 0.95, 1.0) * (ring * 0.8 + exp (-dp * dp * 150.0) * 0.35);
    float fade = 1.0 - smoothstep (1.0, 1.4, edge);
    fragColor = vec4 (col * fade, 1.0);
})";

    const char* spriteVS = R"(#version 150
in vec3 aPos; in vec4 aCol; in float aSize; out vec4 vCol;
uniform mat4 uViewProj; uniform float uViewportH;
void main()
{
    vCol = aCol;
    gl_Position = uViewProj * vec4 (aPos, 1.0);
    gl_PointSize = clamp (aSize * uViewportH / max (gl_Position.w, 0.05), 1.0, 256.0);
})";

    const char* spriteFS = R"(#version 150
in vec4 vCol; out vec4 fragColor;
void main()
{
    vec2 c = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot (c, c);
    if (r2 > 1.0) discard;
    float a = exp (-r2 * 4.5) * 0.85 + exp (-r2 * 45.0) * 0.9;
    fragColor = vec4 (vCol.rgb * vCol.a * a, 1.0);
})";

    const char* ribbonVS = R"(#version 150
in vec3 aPos; in vec2 aUv; in vec4 aCol; out vec2 vUv; out vec4 vCol;
uniform mat4 uViewProj;
void main() { vUv = aUv; vCol = aCol; gl_Position = uViewProj * vec4 (aPos, 1.0); })";

    const char* ribbonFS = R"(#version 150
in vec2 vUv; in vec4 vCol; out vec4 fragColor; uniform float uTime;
void main()
{
    float across = 1.0 - abs (vUv.y * 2.0 - 1.0);
    float core = pow (across, 6.0);
    float glow = pow (across, 1.6) * 0.3;
    float flow = 0.65 + 0.35 * sin (vUv.x * 22.0 - uTime * 5.0);
    fragColor = vec4 (vCol.rgb * vCol.a * (core + glow) * flow, 1.0);
})";

    ConstellationView::V3 rgb (juce::Colour c) { return { c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue() }; }

    void perspective (float* m, float fovY, float aspect, float zn, float zf)
    {
        const float f = 1.0f / std::tan (fovY * 0.5f);
        std::fill (m, m + 16, 0.0f);
        m[0] = f / aspect; m[5] = f; m[10] = (zf + zn) / (zn - zf); m[11] = -1.0f; m[14] = 2.0f * zf * zn / (zn - zf);
    }

    void lookAt (float* m, V3 eye, V3 f, V3 r, V3 u)
    {
        m[0] = r.x; m[4] = r.y; m[8]  = r.z; m[12] = -dot (r, eye);
        m[1] = u.x; m[5] = u.y; m[9]  = u.z; m[13] = -dot (u, eye);
        m[2] = -f.x; m[6] = -f.y; m[10] = -f.z; m[14] = dot (f, eye);
        m[3] = 0; m[7] = 0; m[11] = 0; m[15] = 1;
    }

    void mul (float* out, const float* a, const float* b)
    {
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
            {
                float s = 0;
                for (int k = 0; k < 4; ++k) s += a[k * 4 + r] * b[c * 4 + k];
                out[c * 4 + r] = s;
            }
    }

    std::unique_ptr<juce::OpenGLShaderProgram> makeProgram (juce::OpenGLContext& ctx, const char* vs, const char* fs)
    {
        auto p = std::make_unique<juce::OpenGLShaderProgram> (ctx);
        if (p->addVertexShader (vs) && p->addFragmentShader (fs) && p->link())
            return p;
        DBG ("Airverbs shader error: " << p->getLastError());
        return nullptr;
    }
}

ConstellationView::ConstellationView (AirverbsProcessor& p) : processor (p)
{
    setOpaque (true);
    context.setOpenGLVersionRequired (juce::OpenGLContext::openGL3_2);
    juce::OpenGLPixelFormat fmt;
    fmt.multisamplingLevel = 4;
    context.setPixelFormat (fmt);
    context.setMultisamplingEnabled (true);
    context.setRenderer (this);
    context.setComponentPaintingEnabled (true);
    context.setContinuousRepainting (true);
    context.attachTo (*this);

    auto& ui = processor.uiState;
    yaw = (float) ui.getProperty ("camYaw", 0.0f);
    pitch = (float) ui.getProperty ("camPitch", 0.62f);
    distance = (float) ui.getProperty ("camDist", 4.1f);
    particles.reserve (2400);
    startTimerHz (30);
}

ConstellationView::~ConstellationView()
{
    stopTimer();
    processor.uiState.setProperty ("camYaw", yaw.load(), nullptr);
    processor.uiState.setProperty ("camPitch", pitch.load(), nullptr);
    processor.uiState.setProperty ("camDist", distance.load(), nullptr);
    context.detach();
}

ConstellationView::V3 ConstellationView::starPos (int k) const
{
    return { (k % 2 == 0) ? -1.0f : 1.0f, 0.3f, (k < 2) ? -1.0f : 1.0f };
}

ConstellationView::Camera ConstellationView::computeCamera (float time, float aspect) const
{
    Camera c;
    const float drift = std::sin (time * 0.07f) * 0.09f;
    const float yw = yaw.load() + drift, pt = juce::jlimit (0.15f, 1.45f, pitch.load() + std::sin (time * 0.05f) * 0.02f);
    const float d = distance.load();
    c.target = { 0.0f, 0.08f, 0.12f };
    c.eye = { c.target.x + d * std::sin (yw) * std::cos (pt), c.target.y + d * std::sin (pt), c.target.z + d * std::cos (yw) * std::cos (pt) };
    c.forward = norm (c.target - c.eye);
    c.right = norm (cross (c.forward, { 0, 1, 0 }));
    c.up = cross (c.right, c.forward);
    c.aspect = aspect;
    c.fovY = 0.72f;
    float proj[16], view[16];
    perspective (proj, c.fovY, aspect, 0.05f, 50.0f);
    lookAt (view, c.eye, c.forward, c.right, c.up);
    mul (c.viewProj.m, proj, view);
    return c;
}

// --------------------------------------------------------------------------------- GL lifecycle
void ConstellationView::newOpenGLContextCreated()
{
    skyProg    = makeProgram (context, skyVS, skyFS);
    floorProg  = makeProgram (context, floorVS, floorFS);
    spriteProg = makeProgram (context, spriteVS, spriteFS);
    ribbonProg = makeProgram (context, ribbonVS, ribbonFS);

    glGenVertexArrays (1, &vao);
    glGenBuffers (1, &quadVbo);
    glGenBuffers (1, &floorVbo);
    glGenBuffers (1, &spriteVbo);
    glGenBuffers (1, &ribbonVbo);

    const float quad[] = { -1, -1, 1, -1, -1, 1, 1, 1 };
    glBindBuffer (GL_ARRAY_BUFFER, quadVbo);
    glBufferData (GL_ARRAY_BUFFER, sizeof (quad), quad, GL_STATIC_DRAW);
    const float e = 1.45f;
    const float floorQuad[] = { -e, 0, -e, e, 0, -e, -e, 0, e, e, 0, e };
    glBindBuffer (GL_ARRAY_BUFFER, floorVbo);
    glBufferData (GL_ARRAY_BUFFER, sizeof (floorQuad), floorQuad, GL_STATIC_DRAW);
    glBindBuffer (GL_ARRAY_BUFFER, 0);

    glReady = skyProg && floorProg && spriteProg && ribbonProg;
    lastTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
}

void ConstellationView::openGLContextClosing()
{
    skyProg.reset(); floorProg.reset(); spriteProg.reset(); ribbonProg.reset();
    for (auto* b : { &quadVbo, &floorVbo, &spriteVbo, &ribbonVbo })
        if (*b != 0) { glDeleteBuffers (1, b); *b = 0; }
    if (vao != 0) { glDeleteVertexArrays (1, &vao); vao = 0; }
    glReady = false;
}

static void bindAttrib (juce::OpenGLShaderProgram& prog, const char* name, int size, int stride, size_t offset)
{
    const GLint loc = glGetAttribLocation (prog.getProgramID(), name);
    if (loc < 0) return;
    glEnableVertexAttribArray ((GLuint) loc);
    glVertexAttribPointer ((GLuint) loc, size, GL_FLOAT, GL_FALSE, stride, (const void*) offset);
}

void ConstellationView::renderOpenGL()
{
    const float scale = (float) context.getRenderingScale();
    const int w = juce::roundToInt (scale * (float) getWidth()), h = juce::roundToInt (scale * (float) getHeight());
    glViewport (0, 0, w, h);
    glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (! glReady || w <= 0 || h <= 0) return;

    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = (float) juce::jlimit (0.0, 0.1, now - lastTime);
    lastTime = now;
    const bool frozen = processor.apvts.getRawParameterValue (ids::freeze)->load() > 0.5f;
    animTime += dt * (frozen ? 0.25f : 1.0f);

    cam = computeCamera (animTime, (float) w / (float) h);
    {
        const juce::SpinLock::ScopedLockType sl (camLock);
        sharedCam = cam;
        sharedTime = animTime;
    }
    buildScene (dt);

    glBindVertexArray (vao);
    glDisable (GL_DEPTH_TEST);
    glEnable (GL_BLEND);
    glEnable (GL_PROGRAM_POINT_SIZE);

    // sky
    glBlendFunc (GL_ONE, GL_ZERO);
    skyProg->use();
    skyProg->setUniform ("uTime", animTime);
    skyProg->setUniform ("uFreeze", frozen ? 1.0f : 0.0f);
    skyProg->setUniform ("uScale", scale);
    const int topo = (int) processor.apvts.getRawParameterValue (ids::topology)->load();
    const auto tint = palette::star (topo % 4);
    skyProg->setUniform ("uTint", tint.getFloatRed(), tint.getFloatGreen(), tint.getFloatBlue());
    glBindBuffer (GL_ARRAY_BUFFER, quadVbo);
    bindAttrib (*skyProg, "aPos", 2, 2 * sizeof (float), 0);
    glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);

    glBlendFunc (GL_ONE, GL_ONE); // everything else is light

    // floor
    auto& tel = processor.network.telemetry;
    floorProg->use();
    glUniformMatrix4fv (glGetUniformLocation (floorProg->getProgramID(), "uViewProj"), 1, GL_FALSE, cam.viewProj.m);
    float sp[12], sc[12], se[4], sw[4];
    for (int k = 0; k < 4; ++k)
    {
        auto p = starPos (k); auto c = rgb (palette::star (k));
        sp[k * 3] = p.x; sp[k * 3 + 1] = p.y; sp[k * 3 + 2] = p.z;
        sc[k * 3] = c.x; sc[k * 3 + 1] = c.y; sc[k * 3 + 2] = c.z;
        se[k] = std::sqrt (tel.energy[(size_t) k].load()) * 0.6f;
        sw[k] = tel.weight[(size_t) k].load();
    }
    const auto fid = floorProg->getProgramID();
    glUniform3fv (glGetUniformLocation (fid, "uStarPos"), 4, sp);
    glUniform3fv (glGetUniformLocation (fid, "uStarCol"), 4, sc);
    glUniform1fv (glGetUniformLocation (fid, "uStarE"), 4, se);
    glUniform1fv (glGetUniformLocation (fid, "uStarW"), 4, sw);
    floorProg->setUniform ("uPuck", tel.posX.load() * 2.0f - 1.0f, tel.posY.load() * 2.0f - 1.0f);
    floorProg->setUniform ("uTime", animTime);
    floorProg->setUniform ("uFreeze", frozen ? 1.0f : 0.0f);
    glBindBuffer (GL_ARRAY_BUFFER, floorVbo);
    bindAttrib (*floorProg, "aPos", 3, 3 * sizeof (float), 0);
    glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);

    // ribbons (beams, tethers, trail, orbit path)
    if (! ribbons.empty())
    {
        ribbonProg->use();
        glUniformMatrix4fv (glGetUniformLocation (ribbonProg->getProgramID(), "uViewProj"), 1, GL_FALSE, cam.viewProj.m);
        ribbonProg->setUniform ("uTime", animTime);
        glBindBuffer (GL_ARRAY_BUFFER, ribbonVbo);
        glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (ribbons.size() * sizeof (RibbonVertex)), ribbons.data(), GL_STREAM_DRAW);
        const int stride = sizeof (RibbonVertex);
        bindAttrib (*ribbonProg, "aPos", 3, stride, 0);
        bindAttrib (*ribbonProg, "aUv", 2, stride, 3 * sizeof (float));
        bindAttrib (*ribbonProg, "aCol", 4, stride, 5 * sizeof (float));
        glDrawArrays (GL_TRIANGLES, 0, (GLsizei) ribbons.size());
    }

    // sprites (stars, particles, photons, puck)
    if (! sprites.empty())
    {
        spriteProg->use();
        glUniformMatrix4fv (glGetUniformLocation (spriteProg->getProgramID(), "uViewProj"), 1, GL_FALSE, cam.viewProj.m);
        spriteProg->setUniform ("uViewportH", (float) h);
        glBindBuffer (GL_ARRAY_BUFFER, spriteVbo);
        glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (sprites.size() * sizeof (SpriteVertex)), sprites.data(), GL_STREAM_DRAW);
        const int stride = sizeof (SpriteVertex);
        bindAttrib (*spriteProg, "aPos", 3, stride, 0);
        bindAttrib (*spriteProg, "aCol", 4, stride, 3 * sizeof (float));
        bindAttrib (*spriteProg, "aSize", 1, stride, 7 * sizeof (float));
        glDrawArrays (GL_POINTS, 0, (GLsizei) sprites.size());
    }

    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindVertexArray (0);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // leave state friendly for JUCE's component painting

    if (captureRequested.exchange (false))
    {
        std::vector<juce::uint8> px ((size_t) (w * h * 4));
        glReadPixels (0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        juce::Image img (juce::Image::ARGB, w, h, true);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const auto* q = px.data() + ((size_t) (h - 1 - y) * (size_t) w + (size_t) x) * 4;
                bd.setPixelColour (x, y, juce::Colour (q[0], q[1], q[2], (juce::uint8) 255));
            }
        const juce::SpinLock::ScopedLockType sl (camLock);
        captured = img;
    }
}

// --------------------------------------------------------------------------------- scene building
void ConstellationView::addSprite (const V3& p, float size, juce::Colour c, float alpha)
{
    sprites.push_back ({ p.x, p.y, p.z, c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue(), alpha, size });
}

void ConstellationView::addRibbon (const V3& a, const V3& b, float width, juce::Colour c, float intensity, float uOffset)
{
    if (intensity <= 0.002f) return;
    const V3 dir = b - a;
    const V3 mid = lerp (a, b, 0.5f);
    const V3 side = norm (cross (dir, cam.eye - mid)) * (width * 0.5f);
    const float l = len (dir);
    const float r = c.getFloatRed(), g = c.getFloatGreen(), bl = c.getFloatBlue();
    const RibbonVertex v0 { a.x - side.x, a.y - side.y, a.z - side.z, uOffset, 0, r, g, bl, intensity };
    const RibbonVertex v1 { a.x + side.x, a.y + side.y, a.z + side.z, uOffset, 1, r, g, bl, intensity };
    const RibbonVertex v2 { b.x - side.x, b.y - side.y, b.z - side.z, uOffset + l, 0, r, g, bl, intensity };
    const RibbonVertex v3 { b.x + side.x, b.y + side.y, b.z + side.z, uOffset + l, 1, r, g, bl, intensity };
    ribbons.insert (ribbons.end(), { v0, v1, v2, v1, v3, v2 });
}

void ConstellationView::buildScene (float dt)
{
    sprites.clear();
    ribbons.clear();
    auto& apvts = processor.apvts;
    auto& tel = processor.network.telemetry;
    auto raw = [&] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };

    const bool frozen = raw (ids::freeze) > 0.5f;
    const int topo = juce::jlimit (0, (int) Topology::count - 1, (int) raw (ids::topology));
    const float morph = raw (ids::morph);
    const float input = std::sqrt (tel.inputLevel.load());
    const float decayAmt = juce::jlimit (0.1f, 2.0f, 0.6f + 0.5f * raw (ids::decay) + raw (ids::feedback) * 0.8f);
    const juce::Colour white (0xffe6f4ff);
    std::uniform_real_distribution<float> uni (-1.0f, 1.0f);

    std::array<float, 4> energy {}, weight {};
    for (int k = 0; k < 4; ++k)
    {
        energy[(size_t) k] = std::sqrt (tel.energy[(size_t) k].load());
        weight[(size_t) k] = tel.weight[(size_t) k].load();
    }

    // ---- puck + tethers + trail
    const V3 puck { tel.posX.load() * 2.0f - 1.0f, 0.03f, tel.posY.load() * 2.0f - 1.0f };
    if (trail.empty() || len (trail.back() - puck) > 0.004f) trail.push_back (puck);
    if (trail.size() > 140) trail.erase (trail.begin(), trail.begin() + (long) (trail.size() - 140));
    for (size_t i = 1; i < trail.size(); ++i)
    {
        const float t = (float) i / (float) trail.size();
        addRibbon (trail[i - 1], trail[i], 0.02f * t, white, 0.35f * t);
    }
    for (int k = 0; k < 4; ++k)
    {
        const auto c = palette::star (k);
        addRibbon (puck, starPos (k), 0.012f, c, 0.12f + 0.55f * weight[(size_t) k]);
    }
    addSprite (puck, 0.16f, white, 0.9f);
    addSprite (puck + V3 { 0, 0.01f, 0 }, 0.05f, juce::Colours::white, 1.0f);

    // ---- orbit path preview
    const float depth = raw (ids::orbitDepth);
    if (depth > 0.01f)
    {
        const int shape = (int) raw (ids::orbitShape);
        const float bx = raw (ids::vecX) * 2.0f - 1.0f, bz = raw (ids::vecY) * 2.0f - 1.0f;
        V3 prev {};
        for (int i = 0; i <= 96; ++i)
        {
            const float ph = (float) i / 96.0f * juce::MathConstants<float>::twoPi;
            float ox = 0, oz = 0;
            switch (shape)
            {
                case 0: ox = std::cos (ph); oz = std::sin (ph); break;
                case 1: ox = std::sin (ph); oz = std::sin (2.0f * ph); break;
                case 2: ox = std::sin (3.0f * ph); oz = std::sin (2.0f * ph + 0.5f); break;
                case 4: ox = std::sin (ph); oz = 0.0f; break;
                default: ox = std::cos (ph) * 0.7f; oz = std::sin (ph) * 0.7f; break; // drift: show its reach
            }
            const V3 p { juce::jlimit (-1.0f, 1.0f, bx + depth * ox), 0.006f, juce::jlimit (-1.0f, 1.0f, bz + depth * oz) };
            if (i > 0) addRibbon (prev, p, 0.008f, juce::Colour (0xff9fe7ff), 0.25f);
            prev = p;
        }
    }

    // ---- topology beams (node 4 = input core)
    struct Beam { int from, to; float intensity; };
    std::vector<Beam> beams;
    const float transient = tel.transient.load();
    switch ((Topology) topo)
    {
        case Topology::Nebula:
            for (int k = 0; k < 4; ++k) beams.push_back ({ 4, k, 1.0f });
            for (int a = 0; a < 4; ++a) for (int b = a + 1; b < 4; ++b) beams.push_back ({ a, b, morph * 0.7f });
            break;
        case Topology::Chain:  beams = { { 4, 0, 1 }, { 0, 1, 1 }, { 1, 2, 1 }, { 2, 3, 1 } }; break;
        case Topology::Twin:   beams = { { 4, 0, 1 }, { 0, 1, 1 }, { 4, 2, 1 }, { 2, 3, 1 } }; break;
        case Topology::Bloom:  beams = { { 4, 0, 1 }, { 0, 1, 1 }, { 0, 2, 1 }, { 0, 3, 1 }, { 4, 1, morph * 0.6f }, { 4, 2, morph * 0.6f }, { 4, 3, morph * 0.6f } }; break;
        case Topology::Prism:  for (int k = 0; k < 4; ++k) beams.push_back ({ 4, k, 0.8f }); break;
        case Topology::Dimension: beams = { { 4, 0, 1 }, { 4, 1, 1 }, { 4, 2, 0.5f + morph * 0.5f }, { 4, 3, 0.5f + morph * 0.5f } }; break;
        case Topology::Pulsar: beams = { { 4, 0, 0.15f + transient }, { 4, 1, 0.15f + transient }, { 4, 2, 1.0f - transient * 0.8f }, { 4, 3, 1.0f - transient * 0.8f } }; break;
        default: break;
    }
    auto nodePos = [&] (int n) { return n == 4 ? corePos() : starPos (n); };
    auto nodeCol = [&] (int n) { return n == 4 ? white : palette::star (n); };
    const float signal = 0.25f + juce::jlimit (0.0f, 1.0f, input * 3.0f);
    for (auto& b : beams)
    {
        const V3 a = nodePos (b.from), c = nodePos (b.to);
        const auto col = nodeCol (b.to).interpolatedWith (nodeCol (b.from), 0.35f);
        addRibbon (a, c, 0.03f, col, b.intensity * (0.25f + 0.5f * signal));
        const int photons = frozen ? 2 : 4;
        for (int i = 0; i < photons; ++i)
        {
            const float t = std::fmod (animTime * (0.25f + signal * 0.4f) + (float) i / (float) photons, 1.0f);
            addSprite (lerp (a, c, t), 0.05f + 0.04f * signal, col, b.intensity * signal * std::sin (t * juce::MathConstants<float>::pi));
        }
    }

    // ---- input core
    addSprite (corePos(), 0.25f + input * 0.6f, white, 0.5f + juce::jmin (0.5f, input * 2.0f));
    addSprite (corePos(), 0.07f, juce::Colours::white, 0.9f);

    // ---- stars
    for (int k = 0; k < 4; ++k)
    {
        const auto c = palette::star (k);
        const bool muted = raw (ids::mute (k)) > 0.5f;
        const float e = energy[(size_t) k];
        const float pulse = 0.85f + 0.15f * std::sin (animTime * 2.0f + (float) k * 1.7f);
        addSprite (starPos (k), (0.45f + e * 1.6f) * pulse, c, muted ? 0.12f : 0.45f + weight[(size_t) k] * 0.4f);
        addSprite (starPos (k), 0.12f + e * 0.3f, c.brighter (0.8f), muted ? 0.2f : 1.0f);
        // reflection on the floor
        addSprite ({ starPos (k).x, 0.0f, starPos (k).z }, 0.3f, c, 0.12f);

        // emission
        if (! frozen && ! muted)
        {
            emitAcc[(size_t) k] += dt * juce::jlimit (0.0f, 160.0f, e * 900.0f + weight[(size_t) k] * 3.0f);
            while (emitAcc[(size_t) k] >= 1.0f && particles.size() < 2200)
            {
                emitAcc[(size_t) k] -= 1.0f;
                Particle p;
                p.star = k;
                p.pos = starPos (k) + V3 { uni (rng) * 0.05f, uni (rng) * 0.05f, uni (rng) * 0.05f };
                const V3 outward = norm (V3 { starPos (k).x, 0.0f, starPos (k).z }) * -1.0f;
                p.vel = V3 { uni (rng) * 0.25f, 0.18f + 0.15f * uni (rng), uni (rng) * 0.25f } + outward * 0.12f;
                p.maxLife = (1.2f + 2.8f * decayAmt) * (0.7f + 0.3f * uni (rng));
                p.size = 0.03f + 0.03f * (uni (rng) * 0.5f + 0.5f);
                p.seed = uni (rng) * 10.0f;
                particles.push_back (p);
            }
            emitAcc[(size_t) k] = std::min (emitAcc[(size_t) k], 4.0f);
        }
    }

    // ---- particles
    for (auto& p : particles)
    {
        if (frozen)
            p.vel = p.vel * (1.0f - std::min (1.0f, dt * 3.0f));
        else
        {
            const V3 rel = p.pos - starPos (p.star);
            const V3 swirl = cross ({ 0, 1, 0 }, rel) * 0.9f;
            p.vel = p.vel + (swirl + V3 { 0, 0.05f, 0 }) * dt;
            p.vel = p.vel * (1.0f - std::min (1.0f, dt * 0.35f));
            p.life += dt;
        }
        p.pos = p.pos + p.vel * dt;
    }
    particles.erase (std::remove_if (particles.begin(), particles.end(), [] (const Particle& p) { return p.life >= p.maxLife || p.pos.y > 3.0f; }), particles.end());
    for (auto& p : particles)
    {
        const float t = p.life / p.maxLife;
        float a = std::min (1.0f, p.life * 8.0f + (frozen ? 1.0f : 0.0f)) * (1.0f - t) * 0.75f;
        if (frozen) a *= 0.6f + 0.4f * std::sin (animTime * 6.0f + p.seed);
        addSprite (p.pos, p.size, frozen ? palette::star (p.star).interpolatedWith (palette::freeze, 0.6f) : palette::star (p.star), a);
    }
}

// --------------------------------------------------------------------------------- overlay + interaction
juce::Point<float> ConstellationView::project (const V3& p) const
{
    Camera c;
    { const juce::SpinLock::ScopedLockType sl (camLock); c = sharedCam; }
    const float* m = c.viewProj.m;
    const float x = m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12];
    const float y = m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13];
    const float w = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
    if (w <= 0.01f) return { -1000, -1000 };
    return { (x / w * 0.5f + 0.5f) * (float) getWidth(), (1.0f - (y / w * 0.5f + 0.5f)) * (float) getHeight() };
}

void ConstellationView::paint (juce::Graphics& g)
{
    // Painted on top of the GL scene.
    const float s = (float) getWidth() / 500.0f;
    for (int k = 0; k < 4; ++k)
    {
        auto pt = project (starPos (k) + V3 { 0, 0.22f, 0 });
        const auto& info = EngineRegistry::get().info ((int) processor.apvts.getRawParameterValue (ids::engine (k))->load());
        const auto c = palette::star (k);
        g.setFont (AirverbsLookAndFeel::font (13.0f * s, true));
        auto r = juce::Rectangle<float> (150 * s, 16 * s).withCentre (pt)
                     .constrainedWithin (getLocalBounds().toFloat().reduced (6 * s));
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawText (ids::slotName (k) + "  " + juce::String (info.name), r.translated (0, 1), juce::Justification::centred);
        g.setColour (c.brighter (0.4f));
        g.drawText (ids::slotName (k) + "  " + juce::String (info.name), r, juce::Justification::centred);
    }

    const int topo = (int) processor.apvts.getRawParameterValue (ids::topology)->load();
    auto bottom = getLocalBounds().toFloat().reduced (12 * s, 8 * s).removeFromBottom (18 * s);
    g.setFont (AirverbsLookAndFeel::font (12.0f * s, true).withExtraKerningFactor (0.15f));
    g.setColour (palette::accent.withAlpha (0.85f));
    g.drawText (juce::String (topologyName (topo)).toUpperCase(), bottom, juce::Justification::centredLeft);
    g.setFont (AirverbsLookAndFeel::font (11.0f * s));
    g.setColour (palette::textDim.withAlpha (0.8f));
    g.drawText ("drag: listener   right-drag: orbit   scroll: zoom", bottom, juce::Justification::centredRight);

    if (processor.apvts.getRawParameterValue (ids::freeze)->load() > 0.5f)
    {
        g.setFont (AirverbsLookAndFeel::font (13.0f * s, true).withExtraKerningFactor (0.4f));
        g.setColour (palette::freeze);
        g.drawText ("FROZEN", getLocalBounds().toFloat().reduced (12 * s).removeFromTop (18 * s), juce::Justification::centredRight);
    }
}

bool ConstellationView::screenToFloor (juce::Point<float> p, float& fx, float& fz) const
{
    Camera c;
    { const juce::SpinLock::ScopedLockType sl (camLock); c = sharedCam; }
    const float nx = p.x / (float) getWidth() * 2.0f - 1.0f;
    const float ny = 1.0f - p.y / (float) getHeight() * 2.0f;
    const float th = std::tan (c.fovY * 0.5f);
    const V3 dir = norm (c.forward + c.right * (nx * th * c.aspect) + c.up * (ny * th));
    if (dir.y > -1.0e-4f) return false;
    const float t = -c.eye.y / dir.y;
    fx = c.eye.x + dir.x * t;
    fz = c.eye.z + dir.z * t;
    return true;
}

void ConstellationView::setVector (float fx, float fz)
{
    for (auto [id, v] : { std::pair<juce::String, float> { ids::vecX, fx }, { ids::vecY, fz } })
        if (auto* p = processor.apvts.getParameter (id))
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (v + 1.0f) * 0.5f));
}

void ConstellationView::mouseDown (const juce::MouseEvent& e)
{
    lastMouse = e.position;
    if (e.mods.isPopupMenu() || e.mods.isAltDown())
    {
        drag = Drag::Camera;
        dragYaw = yaw.load(); dragPitch = pitch.load();
        return;
    }
    float fx, fz;
    if (screenToFloor (e.position, fx, fz))
    {
        drag = Drag::Puck;
        processor.apvts.getParameter (ids::vecX)->beginChangeGesture();
        processor.apvts.getParameter (ids::vecY)->beginChangeGesture();
        setVector (fx, fz);
    }
}

void ConstellationView::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == Drag::Camera)
    {
        const auto d = e.position - lastMouse;
        yaw = dragYaw - d.x * 0.008f;
        pitch = juce::jlimit (0.15f, 1.45f, dragPitch + d.y * 0.006f);
    }
    else if (drag == Drag::Puck)
    {
        float fx, fz;
        if (screenToFloor (e.position, fx, fz)) setVector (fx, fz);
    }
}

void ConstellationView::mouseUp (const juce::MouseEvent&)
{
    if (drag == Drag::Puck)
    {
        processor.apvts.getParameter (ids::vecX)->endChangeGesture();
        processor.apvts.getParameter (ids::vecY)->endChangeGesture();
    }
    drag = Drag::None;
}

void ConstellationView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || e.mods.isAltDown())
    {
        yaw = 0.0f; pitch = 0.62f; distance = 4.1f;
        return;
    }
    processor.history.beginAction ("Centre listener");
    for (auto& id : { ids::vecX, ids::vecY })
        if (auto* p = processor.apvts.getParameter (id))
        {
            p->beginChangeGesture(); p->setValueNotifyingHost (0.5f); p->endChangeGesture();
        }
    processor.history.commitNow();
}

void ConstellationView::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    distance = juce::jlimit (2.2f, 6.5f, distance.load() * (1.0f - w.deltaY * 0.25f));
}

} // namespace airverbs

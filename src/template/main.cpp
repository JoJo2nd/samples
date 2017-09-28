#define ENTRY_CONFIG_IMPLEMENT_MAIN (1)

#include <bx/uint32_t.h>
#include "common.h"
#include "bgfx_utils.h"
#include "camera.h"
#include <stdio.h>

#define RENDER_PASS_SOLID           (0)

#define MESH_ASSET_PATH ("data/lostempire.mesh")
#define VS_ASSET_PATH ("data/simple.vs")
#define FS_ASSET_PATH ("data/simple.ps")

static float s_texelHalf = 0.0f;

struct PosTexCoord0Vertex
{
    float m_x;
    float m_y;
    float m_z;
    float m_u;
    float m_v;

    static void init()
    {
        ms_decl
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
    }

    static bgfx::VertexDecl ms_decl;
};

bgfx::VertexDecl PosTexCoord0Vertex::ms_decl;

static const bgfx::Memory* loadMem(const char* filePath)
{
    FILE* f = fopen(filePath, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        uint32_t size = (uint32_t)ftell(f);
        fseek(f, 0, SEEK_SET);
        const bgfx::Memory* mem = bgfx::alloc(size + 1);
        fread(mem->data, 1, size, f);
        fclose(f);
        mem->data[mem->size - 1] = '\0';
        return mem;
    }

    DBG("Failed to load %s.", filePath);
    return NULL;
}

void screenSpaceQuad(float _textureWidth, float _textureHeight, float _texelHalf, bool _originBottomLeft, float _width = 1.0f, float _height = 1.0f)
{
    if (bgfx::getAvailTransientVertexBuffer(3, PosTexCoord0Vertex::ms_decl))
    {
        bgfx::TransientVertexBuffer vb;
        bgfx::allocTransientVertexBuffer(&vb, 3, PosTexCoord0Vertex::ms_decl);
        PosTexCoord0Vertex* vertex = (PosTexCoord0Vertex*)vb.data;

        const float minx = -_width;
        const float maxx = _width;
        const float miny = 0.0f;
        const float maxy = _height*2.0f;

        const float texelHalfW = _texelHalf / _textureWidth;
        const float texelHalfH = _texelHalf / _textureHeight;
        const float minu = -1.0f + texelHalfW;
        const float maxu = 1.0f + texelHalfH;

        const float zz = 0.0f;

        float minv = texelHalfH;
        float maxv = 2.0f + texelHalfH;

        if (_originBottomLeft)
        {
            float temp = minv;
            minv = maxv;
            maxv = temp;

            minv -= 1.0f;
            maxv -= 1.0f;
        }

        vertex[0].m_x = minx;
        vertex[0].m_y = miny;
        vertex[0].m_z = zz;
        vertex[0].m_u = minu;
        vertex[0].m_v = minv;

        vertex[1].m_x = maxx;
        vertex[1].m_y = miny;
        vertex[1].m_z = zz;
        vertex[1].m_u = maxu;
        vertex[1].m_v = minv;

        vertex[2].m_x = maxx;
        vertex[2].m_y = maxy;
        vertex[2].m_z = zz;
        vertex[2].m_u = maxu;
        vertex[2].m_v = maxv;

        bgfx::setVertexBuffer(0, &vb);
    }
}

class ExampleHelloWorld : public entry::AppI {
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_debug;
    uint32_t m_reset;
    entry::MouseState mouseState;
    float cameraPos[4] = { 0 };

    Mesh* m_mesh;

    bgfx::ProgramHandle solidProg;

    bgfx::Caps const* m_caps;
    
    void init(int _argc, const char* const* _argv, uint32_t _width, uint32_t _height) {
        Args args(_argc, _argv);

        cameraCreate();

        float cameraP[3] = {0.f, 0.f, 0.f};
        cameraSetPosition(cameraP);

        PosTexCoord0Vertex::init();

        m_width = _width;
        m_height = _height;
        m_debug = BGFX_DEBUG_TEXT;
        m_reset = BGFX_RESET_VSYNC;

        bgfx::init(bgfx::RendererType::Direct3D11, args.m_pciId);
        bgfx::reset(m_width, m_height, m_reset);

        const bgfx::RendererType::Enum renderer = bgfx::getRendererType();
        s_texelHalf = bgfx::RendererType::Direct3D9 == renderer ? 0.5f : 0.0f;

        // Get renderer capabilities info.
        m_caps = bgfx::getCaps();

        // Enable debug text.
        bgfx::setDebug(m_debug);

        // Set view 0 clear state.
        bgfx::setViewClear(RENDER_PASS_SOLID
            , BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH
            , 0x303030ff
            , 1.0f
            , 0
            );

        m_mesh = meshLoad(MESH_ASSET_PATH);
        
        {
            bgfx::Memory const* vs = loadMem(VS_ASSET_PATH);
            bgfx::Memory const* ps = loadMem(FS_ASSET_PATH);
            solidProg = bgfx::createProgram(bgfx::createShader(vs), bgfx::createShader(ps), true);
        }
    }

    virtual int shutdown() {
        cameraDestroy();

        meshUnload(m_mesh);
        // Shutdown bgfx.
        bgfx::shutdown();

        return 0;
    }

    bool update() {
        static float colours[][4] = {
            { 1.f, 0.f, 0.f, 1.f },
            { 0.f, 1.f, 0.f, 1.f },
            { 0.f, 0.f, 1.f, 1.f },
        };

        if (!entry::processEvents(m_width, m_height, m_debug, m_reset, &mouseState)) {
            // Time.
            int64_t now = bx::getHPCounter();
            static int64_t last = now;
            const int64_t frameTime = now - last;
            last = now;
            const double freq = double(bx::getHPFrequency());
            const double toMs = 1000.0 / freq;
            const float time = (float)((now) / double(bx::getHPFrequency()));
            const float deltaTime = float(frameTime / freq);

            float view[16], iViewX[16], mTmp[16];
            cameraUpdate(deltaTime, mouseState);
            cameraGetViewMtx(view);
            cameraGetPosition(cameraPos);

            bx::mtxInverse(mTmp, view);
            bx::mtxTranspose(iViewX, mTmp);
            bx::mtxIdentity(mTmp);

            float proj[16];
            bx::mtxProj(proj, 60.0f, float(m_width) / float(m_height), 0.5f, 1000.0f, false);

            float orthoProj[16];
            bx::mtxOrtho(orthoProj, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 100.0f, 0.f, false);


            // Set view 0 default viewport.
            bgfx::setViewRect(RENDER_PASS_SOLID, 0, 0, m_width, m_height);

            bgfx::setViewTransform(RENDER_PASS_SOLID, view, proj);

            uint64_t state = 0;
            meshSubmit(m_mesh, RENDER_PASS_SOLID, solidProg, mTmp);

            // Use debug font to print information about this example.
            bgfx::dbgTextClear();
            bgfx::dbgTextPrintf(0, 2, 0x6f, "Description: template.");

            // Advance to next frame. Rendering thread will be kicked to
            // process submitted rendering primitives.
            bgfx::frame();

            return true;
        }

        return false;
    }

public:
	ExampleHelloWorld(const char* _name, const char* _description) : AppI(_name, _description) {}
};

ENTRY_IMPLEMENT_MAIN(ExampleHelloWorld, "blah", "blah");
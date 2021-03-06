#include "mythrender_opengl2.h"

#define LOC QString("OpenGL2: ")
#define LOC_ERR QString("OpenGL2 Error: ")

#define VERTEX_INDEX  0
#define COLOR_INDEX   1
#define TEXTURE_INDEX 2
#define VERTEX_SIZE   2
#define TEXTURE_SIZE  2

static const GLuint kVertexOffset  = 0;
static const GLuint kTextureOffset = 8 * sizeof(GLfloat);
static const GLuint kVertexSize    = 16 * sizeof(GLfloat);

static const QString kDefaultVertexShader =
"GLSL_DEFINES"
"attribute vec2 a_position;\n"
"attribute vec4 a_color;\n"
"attribute vec2 a_texcoord0;\n"
"varying   vec4 v_color;\n"
"varying   vec2 v_texcoord0;\n"
"uniform   mat4 u_projection;\n"
"void main() {\n"
"    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);\n"
"    v_texcoord0 = a_texcoord0;\n"
"    v_color     = a_color;\n"
"}\n";

static const QString kDefaultFragmentShader =
"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture0;\n"
"varying vec4 v_color;\n"
"varying vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    gl_FragColor = GLSL_TEXTURE(s_texture0, v_texcoord0) * v_color;\n"
"}\n";

static const QString kSimpleVertexShader =
"GLSL_DEFINES"
"attribute vec2 a_position;\n"
"attribute vec4 a_color;\n"
"varying   vec4 v_color;\n"
"uniform   mat4 u_projection;\n"
"void main() {\n"
"    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);\n"
"    v_color     = a_color;\n"
"}\n";

static const QString kSimpleFragmentShader =
"GLSL_DEFINES"
"varying vec4 v_color;\n"
"void main(void)\n"
"{\n"
"    gl_FragColor = v_color;\n"
"}\n";

class MythGLShaderObject
{
  public:
    MythGLShaderObject(uint vert, uint frag)
      : m_vertex_shader(vert), m_fragment_shader(frag) { }
    MythGLShaderObject()
      : m_vertex_shader(0), m_fragment_shader(0) { }

    GLuint m_vertex_shader;
    GLuint m_fragment_shader;
};

MythRenderOpenGL2::MythRenderOpenGL2(const QGLFormat& format, QPaintDevice* device)
  : MythRenderOpenGL(format, device)
{
    ResetVars();
    ResetProcs();
}

MythRenderOpenGL2::MythRenderOpenGL2(const QGLFormat& format)
  : MythRenderOpenGL(format)
{
    ResetVars();
    ResetProcs();
}

MythRenderOpenGL2::~MythRenderOpenGL2()
{
    makeCurrent();
    DeleteOpenGLResources();
    doneCurrent();
}

void MythRenderOpenGL2::Init2DState(void)
{
    MythRenderOpenGL::Init2DState();
}

void MythRenderOpenGL2::ResetVars(void)
{
    MythRenderOpenGL::ResetVars();
    memset(m_projection, 0, sizeof(m_projection));
    memset(m_shaders, 0, sizeof(m_shaders));
    m_active_obj = 0;
}

void MythRenderOpenGL2::ResetProcs(void)
{
    MythRenderOpenGL::ResetProcs();

    m_glCreateShader = NULL;
    m_glShaderSource = NULL;
    m_glCompileShader = NULL;
    m_glGetShaderiv = NULL;
    m_glGetShaderInfoLog = NULL;
    m_glDeleteShader = NULL;
    m_glCreateProgram = NULL;
    m_glAttachShader = NULL;
    m_glLinkProgram = NULL;
    m_glUseProgram = NULL;
    m_glGetProgramInfoLog = NULL;
    m_glGetProgramiv = NULL;
    m_glDetachShader = NULL;
    m_glDeleteShader = NULL;
    m_glGetUniformLocation = NULL;
    m_glUniform4f = NULL;
    m_glUniformMatrix4fv = NULL;
    m_glVertexAttribPointer = NULL;
    m_glEnableVertexAttribArray = NULL;
    m_glDisableVertexAttribArray = NULL;
    m_glBindAttribLocation = NULL;
    m_glVertexAttrib4f = NULL;
}

bool MythRenderOpenGL2::InitFeatures(void)
{
    m_exts_supported = kGLFeatNone;

    static bool glslshaders = true;
    static bool check       = true;
    if (check)
    {
        check = false;
        glslshaders = !getenv("OPENGL_NOGLSL");
        if (!glslshaders)
            VERBOSE(VB_GENERAL, LOC + "Disabling GLSL.");
    }

    // These should all be present for a valid OpenGL2.0/ES installation
    if (m_glShaderSource  && m_glCreateShader &&
        m_glCompileShader && m_glGetShaderiv &&
        m_glGetShaderInfoLog && m_glDeleteShader &&
        m_glCreateProgram &&
        m_glAttachShader  && m_glLinkProgram &&
        m_glUseProgram    && m_glGetProgramInfoLog &&
        m_glDetachShader  && m_glGetProgramiv &&
        m_glDeleteShader  && m_glGetUniformLocation &&
        m_glUniform4f     && m_glUniformMatrix4fv &&
        m_glVertexAttribPointer &&
        m_glEnableVertexAttribArray &&
        m_glDisableVertexAttribArray &&
        m_glBindAttribLocation &&
        m_glVertexAttrib4f && glslshaders)
    {
        VERBOSE(VB_GENERAL, LOC + QString("GLSL supported"));
        m_exts_supported += kGLSL;
    }

    MythRenderOpenGL::InitFeatures();

    // After rect texture support
    if (m_exts_supported & kGLSL)
    {
        DeleteDefaultShaders();
        CreateDefaultShaders();
    }

    return true;
}

void MythRenderOpenGL2::InitProcs(void)
{
    MythRenderOpenGL::InitProcs();

    m_qualifiers = QString();

    m_glCreateShader = (MYTH_GLCREATESHADERPROC)
        GetProcAddress("glCreateShader");
    m_glShaderSource = (MYTH_GLSHADERSOURCEPROC)
        GetProcAddress("glShaderSource");
    m_glCompileShader = (MYTH_GLCOMPILESHADERPROC)
        GetProcAddress("glCompileShader");
    m_glGetShaderiv = (MYTH_GLGETSHADERIVPROC)
        GetProcAddress("glGetShaderiv");
    m_glGetShaderInfoLog = (MYTH_GLGETSHADERINFOLOGPROC)
        GetProcAddress("glGetShaderInfoLog");
    m_glDeleteProgram = (MYTH_GLDELETEPROGRAMPROC)
        GetProcAddress("glDeleteProgram");
    m_glCreateProgram = (MYTH_GLCREATEPROGRAMPROC)
        GetProcAddress("glCreateProgram");
    m_glAttachShader = (MYTH_GLATTACHSHADERPROC)
        GetProcAddress("glAttachShader");
    m_glLinkProgram = (MYTH_GLLINKPROGRAMPROC)
        GetProcAddress("glLinkProgram");
    m_glUseProgram = (MYTH_GLUSEPROGRAMPROC)
        GetProcAddress("glUseProgram");
    m_glGetProgramInfoLog = (MYTH_GLGETPROGRAMINFOLOGPROC)
        GetProcAddress("glGetProgramInfoLog");
    m_glGetProgramiv = (MYTH_GLGETPROGRAMIVPROC)
        GetProcAddress("glGetProgramiv");
    m_glDetachShader = (MYTH_GLDETACHSHADERPROC)
        GetProcAddress("glDetachShader");
    m_glDeleteShader = (MYTH_GLDELETESHADERPROC)
        GetProcAddress("glDeleteShader");
    m_glGetUniformLocation = (MYTH_GLGETUNIFORMLOCATIONPROC)
        GetProcAddress("glGetUniformLocation");
    m_glUniform4f = (MYTH_GLUNIFORM4FPROC)
        GetProcAddress("glUniform4f");
    m_glUniformMatrix4fv = (MYTH_GLUNIFORMMATRIX4FVPROC)
        GetProcAddress("glUniformMatrix4fv");
    m_glVertexAttribPointer = (MYTH_GLVERTEXATTRIBPOINTERPROC)
        GetProcAddress("glVertexAttribPointer");
    m_glEnableVertexAttribArray = (MYTH_GLENABLEVERTEXATTRIBARRAYPROC)
        GetProcAddress("glEnableVertexAttribArray");
    m_glDisableVertexAttribArray = (MYTH_GLDISABLEVERTEXATTRIBARRAYPROC)
        GetProcAddress("glDisableVertexAttribArray");
    m_glBindAttribLocation = (MYTH_GLBINDATTRIBLOCATIONPROC)
        GetProcAddress("glBindAttribLocation");
    m_glVertexAttrib4f = (MYTH_GLVERTEXATTRIB4FPROC)
        GetProcAddress("glVertexAttrib4f");
}

uint MythRenderOpenGL2::CreateShaderObject(const QString &vertex,
                                          const QString &fragment)
{
    if (!(m_exts_supported & kGLSL))
        return 0;

    OpenGLLocker locker(this);

    uint result = 0;
    QString vert_shader = vertex.isEmpty() ? kDefaultVertexShader : vertex;
    QString frag_shader = fragment.isEmpty() ? kDefaultFragmentShader: fragment;
    vert_shader.detach();
    frag_shader.detach();

    OptimiseShaderSource(vert_shader);
    OptimiseShaderSource(frag_shader);

    result = m_glCreateProgram();
    if (!result)
        return 0;

    MythGLShaderObject object(CreateShader(GL_VERTEX_SHADER, vert_shader),
                              CreateShader(GL_FRAGMENT_SHADER, frag_shader));
    m_shader_objects.insert(result, object);

    if (!ValidateShaderObject(result))
    {
        DeleteShaderObject(result);
        return 0;
    }

    return result;
}

void MythRenderOpenGL2::DeleteShaderObject(uint obj)
{
    if (!m_shader_objects.contains(obj))
        return;

    makeCurrent();

    GLuint vertex   = m_shader_objects[obj].m_vertex_shader;
    GLuint fragment = m_shader_objects[obj].m_fragment_shader;
    m_glDetachShader(obj, vertex);
    m_glDetachShader(obj, fragment);
    m_glDeleteShader(vertex);
    m_glDeleteShader(fragment);
    m_glDeleteProgram(obj);
    m_shader_objects.remove(obj);

    Flush(true);
    doneCurrent();
}

void MythRenderOpenGL2::EnableShaderObject(uint obj)
{
    if (obj == m_active_obj)
        return;

    if (!obj && m_active_obj)
    {
        makeCurrent();
        m_glUseProgram(0);
        m_active_obj = 0;
        doneCurrent();
        return;
    }

    if (!m_shader_objects.contains(obj))
        return;

    makeCurrent();
    m_glUseProgram(obj);
    m_active_obj = obj;
    doneCurrent();
}

void MythRenderOpenGL2::SetShaderParams(uint obj, void* vals,
                                        const char* uniform)
{
    makeCurrent();
    const float *v = (float*)vals;

    EnableShaderObject(obj);
    GLint loc = m_glGetUniformLocation(obj, uniform);
    m_glUniformMatrix4fv(loc, 1, GL_FALSE, v);
    doneCurrent();
}

void MythRenderOpenGL2::DrawBitmapPriv(uint tex, const QRect *src,
                                       const QRect *dst, uint prog, int alpha,
                                       int red, int green, int blue)
{
    if (prog && !m_shader_objects.contains(prog))
        prog = 0;
    if (prog == 0)
        prog = m_shaders[kShaderDefault];

    EnableShaderObject(prog);
    SetShaderParams(prog, &m_projection[0][0], "u_projection");
    SetBlend(true);

    EnableTextures(tex);
    glBindTexture(m_textures[tex].m_type, tex);

    m_glBindBuffer(GL_ARRAY_BUFFER, m_textures[tex].m_vbo);
    UpdateTextureVertices(tex, src, dst);
    m_glBufferData(GL_ARRAY_BUFFER, kVertexSize, NULL, GL_STREAM_DRAW);
    void* target = m_glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (target)
        memcpy(target, m_textures[tex].m_vertex_data, kVertexSize);
    m_glUnmapBuffer(GL_ARRAY_BUFFER);

    m_glEnableVertexAttribArray(VERTEX_INDEX);
    m_glEnableVertexAttribArray(TEXTURE_INDEX);

    m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                            VERTEX_SIZE * sizeof(GLfloat),
                            (const void *) kVertexOffset);
    m_glVertexAttrib4f(COLOR_INDEX, red / 255.0, green / 255.0, blue / 255.0, alpha / 255.0);
    m_glVertexAttribPointer(TEXTURE_INDEX, TEXTURE_SIZE, GL_FLOAT, GL_FALSE,
                            TEXTURE_SIZE * sizeof(GLfloat),
                            (const void *) kTextureOffset);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_glDisableVertexAttribArray(TEXTURE_INDEX);
    m_glDisableVertexAttribArray(VERTEX_INDEX);
    m_glBindBuffer(GL_ARRAY_BUFFER, 0);
}



void MythRenderOpenGL2::DrawBitmapPriv(uint *textures, uint texture_count,
                                       const QRectF *src, const QRectF *dst,
                                       uint prog)
{
    if (prog && !m_shader_objects.contains(prog))
        prog = 0;
    if (prog == 0)
        prog = m_shaders[kShaderDefault];

    uint first = textures[0];

    EnableShaderObject(prog);
    SetShaderParams(prog, &m_projection[0][0], "u_projection");
    SetBlend(false);

    EnableTextures(first);
    uint active_tex = 0;
    for (uint i = 0; i < texture_count; i++)
    {
        if (m_textures.contains(textures[i]))
        {
            ActiveTexture(GL_TEXTURE0 + active_tex++);
            glBindTexture(m_textures[textures[i]].m_type, textures[i]);
        }
    }

    m_glBindBuffer(GL_ARRAY_BUFFER, m_textures[first].m_vbo);
    UpdateTextureVertices(first, src, dst);
    m_glBufferData(GL_ARRAY_BUFFER, kVertexSize, NULL, GL_STREAM_DRAW);
    void* target = m_glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (target)
        memcpy(target, m_textures[first].m_vertex_data, kVertexSize);
    m_glUnmapBuffer(GL_ARRAY_BUFFER);

    m_glEnableVertexAttribArray(VERTEX_INDEX);
    m_glEnableVertexAttribArray(TEXTURE_INDEX);

    m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                            VERTEX_SIZE * sizeof(GLfloat),
                            (const void *) kVertexOffset);
    m_glVertexAttrib4f(COLOR_INDEX, 1.0, 1.0, 1.0, 1.0);
    m_glVertexAttribPointer(TEXTURE_INDEX, TEXTURE_SIZE, GL_FLOAT, GL_FALSE,
                            TEXTURE_SIZE * sizeof(GLfloat),
                            (const void *) kTextureOffset);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_glDisableVertexAttribArray(TEXTURE_INDEX);
    m_glDisableVertexAttribArray(VERTEX_INDEX);
    m_glBindBuffer(GL_ARRAY_BUFFER, 0);
}



void MythRenderOpenGL2::DrawRectPriv(const QRect &area, bool drawFill,
                                     const QColor &fillColor,  bool drawLine,
                                     int lineWidth, const QColor &lineColor,
                                     int prog)
{
    if (prog && !m_shader_objects.contains(prog))
        prog = 0;
    if (prog == 0)
        prog = m_shaders[kShaderSimple];

    EnableShaderObject(prog);
    SetShaderParams(prog, &m_projection[0][0], "u_projection");
    SetBlend(true);
    DisableTextures();

    m_glEnableVertexAttribArray(VERTEX_INDEX);

    if (drawFill)
    {
        m_glVertexAttrib4f(COLOR_INDEX,
                           fillColor.red() / 255.0,
                           fillColor.green() / 255.0,
                           fillColor.blue() / 255.0,
                           fillColor.alpha() / 255.0);
        GetCachedVBO(GL_TRIANGLE_STRIP, area);
        m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               (const void *) kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        m_glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    if (drawLine)
    {
        glLineWidth(lineWidth);
        m_glVertexAttrib4f(COLOR_INDEX,
                           lineColor.red() / 255.0,
                           lineColor.green() / 255.0,
                           lineColor.blue() / 255.0,
                           lineColor.alpha() / 255.0);
        GetCachedVBO(GL_LINE_LOOP, area);
        m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               (const void *) kVertexOffset);
        glDrawArrays(GL_LINE_LOOP, 0, 4);
        m_glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    m_glDisableVertexAttribArray(VERTEX_INDEX);
}

void MythRenderOpenGL2::CreateDefaultShaders(void)
{
    m_shaders[kShaderSimple]  = CreateShaderObject(kSimpleVertexShader,
                                                   kSimpleFragmentShader);
    m_shaders[kShaderDefault] = CreateShaderObject(kDefaultVertexShader,
                                                   kDefaultFragmentShader);
}

void MythRenderOpenGL2::DeleteDefaultShaders(void)
{
    for (int i = 0; i < kShaderCount; i++)
    {
        DeleteShaderObject(m_shaders[i]);
        m_shaders[i] = 0;
    }
}

uint MythRenderOpenGL2::CreateShader(int type, const QString &source)
{
    uint result = m_glCreateShader(type);
    QByteArray src = source.toAscii();
    const char* tmp[1] = { src.constData() };
    m_glShaderSource(result, 1, tmp, NULL);
    m_glCompileShader(result);
    GLint compiled;
    m_glGetShaderiv(result, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        GLint length = 0;
        m_glGetShaderiv(result, GL_INFO_LOG_LENGTH, &length);
        if (length > 1)
        {
            char *log = (char*)malloc(sizeof(char) * length);
            m_glGetShaderInfoLog(result, length, NULL, log);
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to compile shader.");
            VERBOSE(VB_IMPORTANT, log);
            VERBOSE(VB_IMPORTANT, source);
            free(log);
        }
        m_glDeleteShader(result);
        result = 0;
    }
    return result;
}

bool MythRenderOpenGL2::ValidateShaderObject(uint obj)
{
    if (!m_shader_objects.contains(obj))
        return false;
    if (!m_shader_objects[obj].m_fragment_shader ||
        !m_shader_objects[obj].m_vertex_shader)
        return false;

    m_glAttachShader(obj, m_shader_objects[obj].m_fragment_shader);
    m_glAttachShader(obj, m_shader_objects[obj].m_vertex_shader);
    m_glBindAttribLocation(obj, VERTEX_INDEX,  "a_position");
    m_glBindAttribLocation(obj, COLOR_INDEX,   "a_color");
    m_glBindAttribLocation(obj, TEXTURE_INDEX, "a_texcoord0");
    m_glLinkProgram(obj);
    return CheckObjectStatus(obj);
}

bool MythRenderOpenGL2::CheckObjectStatus(uint obj)
{
    int ok;
    m_glGetProgramiv(obj, GL_OBJECT_LINK_STATUS, &ok);
    if (ok > 0)
        return true;

    VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Failed to link shader object."));
    int infologLength = 0;
    int charsWritten  = 0;
    char *infoLog;
    m_glGetProgramiv(obj, GL_OBJECT_INFO_LOG_LENGTH, &infologLength);
    if (infologLength > 0)
    {
        infoLog = (char *)malloc(infologLength);
        m_glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
        VERBOSE(VB_IMPORTANT, QString("\n\n%1").arg(infoLog));
        free(infoLog);
    }
    return false;
}

void MythRenderOpenGL2::OptimiseShaderSource(QString &source)
{
    QString version = "#version 100\n";
    QString extensions = "";
    QString sampler = "sampler2D";
    QString texture = "texture2D";

    if ((m_exts_used & kGLExtRect) && source.contains("GLSL_SAMPLER"))
    {
        extensions += "#extension GL_ARB_texture_rectangle : enable\n";
        sampler += "Rect";
        texture += "Rect";
    }

    source.replace("GLSL_SAMPLER", sampler);
    source.replace("GLSL_TEXTURE", texture);
    source.replace("GLSL_DEFINES", version + extensions + m_qualifiers);

    VERBOSE(VB_EXTRA, "\n" + source);
}

void MythRenderOpenGL2::DeleteOpenGLResources(void)
{
    VERBOSE(VB_GENERAL, LOC + "Deleting OpenGL Resources");
    DeleteDefaultShaders();
    DeleteShaders();
    MythRenderOpenGL::DeleteOpenGLResources();
}

void MythRenderOpenGL2::SetMatrixView(void)
{
    float right = m_viewport.width();
    float bottom = m_viewport.height();
    memset(m_projection, 0, sizeof(m_projection));
    if (right <= 0 || bottom <= 0)
        return;
    m_projection[0][0] = 2.0 / right;
    m_projection[1][1] = 2.0 / -bottom;
    m_projection[2][2] = 1.0;
    m_projection[3][0] = -1.0;
    m_projection[3][1] = 1.0;
    m_projection[3][3] = 1.0;
}

void MythRenderOpenGL2::DeleteShaders(void)
{
    QHash<GLuint, MythGLShaderObject>::iterator it;
    for (it = m_shader_objects.begin(); it != m_shader_objects.end(); ++it)
    {
        GLuint object   = it.key();
        GLuint vertex   = it.value().m_vertex_shader;
        GLuint fragment = it.value().m_fragment_shader;
        m_glDetachShader(object, vertex);
        m_glDetachShader(object, fragment);
        m_glDeleteShader(vertex);
        m_glDeleteShader(fragment);
        m_glDeleteProgram(object);
    }
    m_shader_objects.clear();
    Flush(true);
}

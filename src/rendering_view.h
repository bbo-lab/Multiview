#ifndef RENDERING_VIEW
#define RENDERING_VIEW


#define GL_GLEXT_PROTOTYPES

#include <iostream>
#include <QtGui/QMouseEvent>
#include <QtGui/QMatrix4x4>
#include <GL/gl.h>
#include <GL/glext.h>
#include <QtGui/QOpenGLTexture>
#include <chrono>
#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QPainter>
#include <QtGui/QOpenGLPaintDevice>
#include "OBJ_Loader.h"
#include "session.h"
#include "io_util.h"
#include "geometry.h"
#include "transformation.h"
#include "image_io.h"
#include "shader.h"
#include "qt_util.h"
#include "openglwindow.h"



typedef std::chrono::time_point<std::chrono::high_resolution_clock> high_res_clock;

class TriangleWindow : public OpenGLWindow
{
public:
    TriangleWindow();

    void mouseMoveEvent(QMouseEvent *e) override;
    session_t session;
    void initialize() override;
    void render() override;
    bool destroyed = false;
    std::vector<arrow_t> arrows;

    high_res_clock last_rendertime;
    std::deque<high_res_clock> last_rendertimes;
    std::deque<high_res_clock> last_screenshottimes;
    spherical_approximation_shader_t approximation_shader;
    perspective_shader_t perspective_shader;
    remapping_spherical_shader_t remapping_shader;
    QOpenGLPaintDevice *qogpd = nullptr;
private:
    std::vector<view_t> views;
    std::vector<QPointF> marker;
    std::vector<QMatrix4x4> camera_transformations;
  
};

void print_models(objl::Loader & Loader, std::ostream & file);

#define BUFFER_OFFSET(i) ((void*)(i))


void load_meshes(mesh_object_t & mesh);

void load_textures(mesh_object_t & mesh);

void destroy(mesh_object_t & mesh);

//TriangleWindow *window;

std::ostream & print_gl_errors(std::ostream & out, std::string const & message, bool endl);

vec3f_t smoothed(std::map<size_t, vec3f_t> const & map, size_t frame, size_t smoothing);

void transform_matrix(object_t const & obj, QMatrix4x4 & matrix, size_t mt_frame, size_t t_smooth, size_t mr_frame, size_t r_smooth);

static const GLfloat g_quad_texture_coords[] = {
    1.0f,  1.0f,
    1.0f, -1.0f,
    -1.0f,  1.0f,
    1.0f, -1.0f,
    -1.0f,  1.0f,
    -1.0f, -1.0f,
};

static const GLfloat g_quad_vertex_buffer_data[] = {
    1.0f,  1.0f,
    1.0f, -1.0f,
    -1.0f,  1.0f,
    1.0f, -1.0f,
    -1.0f,  1.0f,
    -1.0f, -1.0f,
};

void render_cubemap(GLuint *renderedTexture, remapping_spherical_shader_t &);

void render_to_screenshot(screenshot_handle_t & current, GLuint **cubemaps, size_t loglevel, scene_t & scene, remapping_spherical_shader_t & remapping_shader);

GLuint render_to_pixel_buffer(screenshot_handle_t & current, GLuint **cubemaps, size_t loglevel, scene_t & scene, bool debug, remapping_spherical_shader_t & remapping_shader);

std::string getGlErrorString();

void copy_pixel_buffer_to_screenshot(screenshot_handle_t & current, bool debug);

void setShaderBoolean(QOpenGLShaderProgram & prog, GLuint attr, const char *name, bool value);

template <typename T>
T interpolated(std::map<size_t, T> const & map, size_t frame)
{
    auto up = map.lower_bound(frame);
    if (up->first == frame)
    {
        return up->second;
    }
    auto low = up;
    --low;
    //auto up = map.upper_bound(frame);
    float value = static_cast<float>(frame - low->first) / (up->first - low->first);
    //std::cout << value << '=' << '(' << frame  << '-' << low->first << ") / (" << up->first << '-'<< low->first << ')'<< std::endl;
    return interpolate(low->second, up->second, value);
}


rotation_t smoothed(std::map<size_t, rotation_t> const & map, size_t frame, size_t smoothing);

#endif

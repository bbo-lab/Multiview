#include "OBJ_Loader.h"
#include "io_util.h"
#include <atomic>
#include <limits>
#include <immintrin.h>
#include <array>

namespace objl
{
Material::Material() : Ns(0.0f), Ni(0.0f), d(0.0f), illum(0){}

Mesh::Mesh(std::vector<VertexCommon> const & _Vertices, std::vector<uint32_t> const & _Indices) : Vertices(_Vertices), Indices(_Indices){}

void Mesh::swap(Mesh & m)
{
    MeshName.swap(m.MeshName);
    Vertices.swap(m.Vertices);
    Indices.swap(m.Indices);
    std::swap(MeshMaterial, m.MeshMaterial);
}

namespace math
{
    inline static __m128 cross_product( __m128 const& vec0, __m128 const& vec1 ) {
    __m128 tmp0 = _mm_shuffle_ps(vec0,vec0,_MM_SHUFFLE(3,0,2,1));
    __m128 tmp1 = _mm_shuffle_ps(vec1,vec1,_MM_SHUFFLE(3,1,0,2));
    __m128 tmp2 = _mm_mul_ps(tmp0,vec1);
    __m128 tmp3 = _mm_mul_ps(tmp0,tmp1);
    __m128 tmp4 = _mm_shuffle_ps(tmp2,tmp2,_MM_SHUFFLE(3,0,2,1));
    return _mm_sub_ps(tmp3,tmp4);
}
    
    inline __m128 load_vec(const vec3f_t & value)
{
 return _mm_setr_ps(value.x(),value.y(),value.z(),0);
}
    
/*    inline __m128 load_vec(const vec3f_t & value)
{
 __m128 x = _mm_load_ss(&value.x());
 __m128 y = _mm_load_ss(&value.y());
 __m128 z = _mm_load_ss(&value.z());
 __m128 xy = _mm_movelh_ps(x, y);
 return _mm_shuffle_ps(xy, z, _MM_SHUFFLE(2, 0, 2, 0));
}*/
    
    vec3f_t CrossV3(const vec3f_t a, const vec3f_t b)
    {
        /*__m128 result = cross_product(load_vec(a),load_vec(b));
        return vec3f_t(result[0],result[1],result[2]);
        */
        return vec3f_t(a.y() * b.z() - a.z() * b.y(),
            a.z() * b.x() - a.x() * b.z(),
            a.x() * b.y() - a.y() * b.x());
    }
    
    float normdot (const vec3f_t & a, const vec3f_t & b){return dot(a, b) / sqrtf(a.dot() * b.dot());}

    float AngleBetweenV3(const vec3f_t a, const vec3f_t b){return acosf(normdot(a,b));}

    vec3f_t ProjV3(const vec3f_t a, const vec3f_t b){return b * (dot(a, b) / b.dot());}
}

namespace algorithm
{
    // A test to see if P1 is on the same side as P2 of a line segment ab
    bool SameSide(vec3f_t const & p1, vec3f_t const & p2, vec3f_t const & a, vec3f_t const & b);

    // Generate a cross produect normal for a triangle
    vec3f_t GenTriNormal(vec3f_t const & t1, vec3f_t const & t2, vec3f_t const & t3);

    // Check to see if a vec3f_t Point is within a 3 vec3f_t Triangle
    bool inTriangle(vec3f_t const & point, vec3f_t const & tri1, vec3f_t const & tri2, vec3f_t const & tri3);

    // Get element at given index position
    template <class T>
    inline const T & getElement(const std::vector<T> &elements, int64_t idx)
    {
        return (idx < 0 ? elements.end() : elements.begin())[idx];
    }

    template <class T>
    inline T & getElement(std::vector<T> &elements, int64_t idx)
    {
        return (idx < 0 ? elements.end() : elements.begin())[idx];
    }

    template<class Iterator1, class Iterator2> Iterator1 find_first_not_of(
        Iterator1 const& begin1,
        Iterator1 const& end1,
        Iterator2 const& begin2,
        Iterator2 const& end2)
    {
        for(Iterator1 mid1 = begin1; mid1 != end1; ++mid1)
        {
            for(Iterator2 mid2 = begin2; mid2 != end2; ++mid2)
                if(*mid1 == *mid2)
                    goto FOUND;
            return mid1;
            FOUND:;
        }
        return end1;
    }

    bool SameSide(vec3f_t const & p1, vec3f_t const & p2, vec3f_t const & a, vec3f_t const & b)
    {
        vec3f_t ba = b - a, p1a = p1-a, p2a = p2-a;
        return dot(ba, ba)*dot(p1a,p2a)-dot(p1a,ba)*dot(p2a,ba) >= 0;
       // return dot(math::CrossV3(ba, p1a), math::CrossV3(ba, p2a)) >= 0;
    }

    // Generate a cross produect normal for a triangle
    vec3f_t GenTriOrthogonal(vec3f_t const & t1, vec3f_t const & t2, vec3f_t const & t3)
    {
        return math::CrossV3(t2 - t1,t3 - t1);
    }

    // Check to see if a vec3f_t Point is within a 3 vec3f_t Triangle
    bool inTriangle(vec3f_t const & point, vec3f_t const & tri1, vec3f_t const & tri2, vec3f_t const & tri3)
    {
        // Test to see if it is within an infinite prism that the triangle outlines.
        bool within_tri_prisim = SameSide(point, tri1, tri2, tri3) && SameSide(point, tri2, tri1, tri3)
            && SameSide(point, tri3, tri1, tri2);

        if (!within_tri_prisim)
            return false;

        vec3f_t n = GenTriOrthogonal(tri1, tri2, tri3);
        return math::ProjV3(point, n) == vec3f_t(0);
    }
}

template<typename SplitIter, size_t len>
bool read_vec(SplitIter & split_iter, matharray<float, len> & vec)
{
    for (size_t i = 0; i < vec.size(); ++i)
    {
        split_iter.parse(vec[i]);
        ++split_iter;
    }
    return true;
}

template <typename SplitIter>
void create_absolute_path(SplitIter & split_iter, std::string const & folder, std::string & result)
{
    if(split_iter[0] != '/'){result = folder;}
    result += (++split_iter).remaining();
    while (result.back() == 13){result.pop_back();}
}

const int64_t unfilled_pair = std::numeric_limits<int64_t>::max() - 1;

struct VertexParser
{
template <typename InputIter>
std::from_chars_result operator()(InputIter iter, InputIter end, std::array<int64_t, 3> & fVertex){
    std::from_chars_result res = std::from_chars(&*iter,&*end, fVertex[0]);
    if (res.ptr == end || *res.ptr != '/')
    {
        return res;
    }
    ++res.ptr;
    if (res.ptr != end && *res.ptr != '/')
    {
        res = std::from_chars(res.ptr,&*end, fVertex[2]);
    }
    if (res.ptr == end || *res.ptr == ' ')
    {
        return res;
    }
    ++res.ptr;
    if (res.ptr != end)
    {
        res = std::from_chars(res.ptr,&*end, fVertex[1]);
    }
    return res;
}
};

bool Loader::LoadFile(std::string const & Path)
{
    // If the file is not an .obj file return false
    if (Path.substr(Path.size() - 4, 4) != ".obj")
        return false;

    std::ifstream file(Path);

    if (!file.is_open())
        return false;

    LoadedMeshes.clear();
    LoadedVertices = 0;
    LoadedIndices = 0;

    std::vector<vec3f_t> Positions;
    std::vector<vec2us_t> TCoords;
    std::vector<vec3s_t> Normals;
    Positions.reserve(256);
    TCoords.reserve(256);
    Normals.reserve(256);
    Positions.emplace_back(0,0,0);
    Normals.emplace_back(0,0,0);
    TCoords.emplace_back(0,0);

    std::vector<std::string> meshMatNames;
    std::string curMeshMatName;

    bool listening = false;
    Mesh cur_mesh;

    #ifdef OBJL_CONSOLE_OUTPUT
    const uint32_t outputEveryNth = 10000;
    uint32_t outputIndicator = outputEveryNth - 1;
    #endif

    std::vector<uint64_t> tVertInd;
    std::string curline;
    curline.reserve(256);
    std::vector<std::array<int64_t, 3> > indices;
    indices.reserve(4);
    auto split_iter = IO_UTIL::make_split_iterator("", [](char c){return c == ' ' || c == '\t';});
    //auto split_iter2= IO_UTIL::make_split_iterator("", [](char c){return c == '/';});
    size_t linenumber = 0;
    size_t vertex_banks = 3;
    std::vector<std::pair<int64_t,int64_t> > vertex_to_index_and_normal;
    while (std::getline(file, curline))
    {
        #ifdef OBJL_CONSOLE_OUTPUT
        if (++outputIndicator == outputEveryNth)
        {
            if (!cur_mesh.MeshName.empty())
            {
                std::cout
                    << "\n- " << linenumber << ' ' << cur_mesh.MeshName
                    << "\t| vertices > " << Positions.size() - 1
                    << "\t| texcoords > " << TCoords.size() - 1
                    << "\t| normals > " << Normals.size() - 1
                    << "\t| triangles > " << (cur_mesh.Vertices.size() / 3)
                    << "\t| material: " << curMeshMatName;
            }
            outputIndicator = 0;
        }
        ++linenumber;
        #endif
        split_iter.str(curline);
        if (split_iter.size()== 2 && split_iter[0] == 'v')
        {
            if (split_iter[1] == 't')
            {
                float u, v;
                split_iter.increment_and_parse(u);
                split_iter.increment_and_parse(v);
                TCoords.emplace_back(static_cast<uint16_t>(u * std::numeric_limits<uint16_t>::max()),static_cast<uint16_t>(v * std::numeric_limits<uint16_t>::max()));
            }
            else if (split_iter[1] == 'n')
            {
                float x, y, z;
                split_iter.increment_and_parse(x);
                split_iter.increment_and_parse(y);
                split_iter.increment_and_parse(z);
                Normals.emplace_back(static_cast<int16_t>(x * std::numeric_limits<int16_t>::max()),static_cast<int16_t>(y * std::numeric_limits<int16_t>::max()), static_cast<int16_t>(z * std::numeric_limits<int16_t>::max()));
            }
        }
        else if (split_iter.size() == 1)
        {
            if (split_iter[0] == 'v')
            {
                float x, y, z;
                split_iter.increment_and_parse(x);
                split_iter.increment_and_parse(y);
                split_iter.increment_and_parse(z);
                Positions.emplace_back(x,y,z);
            }
            else if (split_iter[0] == 'f')
            {
                size_t oldVertexSize = cur_mesh.Vertices.size();
                size_t oldIndexSize = cur_mesh.Indices.size();
                bool noNormal = false;
                while (true)
                {
                    std::array<int64_t, 3> fVertex({0, 0, 0});
                    VertexParser p;
                    if (!split_iter.increment_and_parse(fVertex, p)){break;}
                    noNormal |= fVertex[1] == 0;
                    indices.emplace_back(fVertex);
                }
                int64_t position_count = Positions.size();
                if (vertex_banks != 0)
                {
                    int64_t texture_coord_count = static_cast<int64_t>(TCoords.size());
                    if (vertex_to_index_and_normal.size() < position_count * vertex_banks)
                    {
                        vertex_to_index_and_normal.resize(std::max(2 * vertex_to_index_and_normal.size(), position_count * vertex_banks), std::make_pair(unfilled_pair, unfilled_pair));
                    }
                    for (std::array<int64_t, 3> & idx : indices)
                    {
                        idx[0] += idx[0] < 0 ? position_count : 0;
                        idx[2] += idx[2] < 0 ? texture_coord_count : 0;
                        auto pair = vertex_to_index_and_normal.begin() + idx[0] * vertex_banks;
                        for (auto last = pair + vertex_banks - 1; true; ++pair)
                        {
                            if (pair->second == idx[2])
                            {
                                break;
                            }
                            if (pair->second == unfilled_pair || pair == last)
                            {
                                pair->first = cur_mesh.Vertices.size();
                                cur_mesh.Vertices.emplace_back(Positions[idx[0]], algorithm::getElement(Normals, idx[1]), TCoords[idx[2]]);
                                pair->second = idx[2];
                                break;
                            }
                        }
                        cur_mesh.Indices.push_back(pair->first);
                    }               
                }
                else
                {
                    for (std::array<int64_t, 3> const & idx : indices)
                    {
                        cur_mesh.Indices.push_back(cur_mesh.Vertices.size());
                        cur_mesh.Vertices.emplace_back(
                            algorithm::getElement(Positions, idx[0]), algorithm::getElement(Normals, idx[1]),  algorithm::getElement(TCoords, idx[2]));
                    }
                }
                if (noNormal)
                {
                    vec3s_t normal = algorithm::GenTriOrthogonal(
                        cur_mesh.Vertices[cur_mesh.Indices[oldIndexSize+1]].Position,
                        cur_mesh.Vertices[cur_mesh.Indices[oldIndexSize+2]].Position,
                        cur_mesh.Vertices[cur_mesh.Indices[oldIndexSize+0]].Position).normalize().convert_normalized<int16_t>();
                    for (auto iter = cur_mesh.Vertices.begin() + oldVertexSize; iter != cur_mesh.Vertices.end(); ++iter)
                    {
                        iter->Normal = normal;
                    }
                }
                LoadedVertices += indices.size();
                LoadedIndices += VertexTriangluation(cur_mesh.Indices, cur_mesh.Vertices, tVertInd, oldIndexSize);
                indices.clear();
            }
            else if (split_iter[0] == 'o' || split_iter[0] == 'g')
            {
                if (listening && !cur_mesh.Indices.empty() && !cur_mesh.Vertices.empty())
                {
                    LoadedMeshes.emplace_back();
                    LoadedMeshes.back().swap(cur_mesh);
                    meshMatNames.emplace_back(curMeshMatName);
                }
                listening = true;
                if ((++split_iter).valid())
                {
                    split_iter.get(cur_mesh.MeshName);
                }
                else
                {
                    cur_mesh.MeshName = "unnamed";
                }
                #ifdef OBJL_CONSOLE_OUTPUT
                std::cout << std::endl;
                outputIndicator = outputEveryNth - 1;
                #endif
            }
        }
        else if (split_iter.size() == 6)
        {
            // Get Mesh Material Name
            if (*split_iter == "usemtl")
            {
                curMeshMatName = (++split_iter).remaining();

                // Create new Mesh, if Material changes within a group
                if (!cur_mesh.Indices.empty() && !cur_mesh.Vertices.empty())
                {
                    std::string tmp = cur_mesh.MeshName;
                    for (size_t i = 1; std::find_if(LoadedMeshes.begin(), LoadedMeshes.end(), [&tmp](Mesh const &m){return m.MeshName == tmp;}) != LoadedMeshes.end(); ++i)
                    {
                        tmp = cur_mesh.MeshName + "_" + std::to_string(i);
                    }
                    cur_mesh.MeshName = tmp;
                    LoadedMeshes.emplace_back();
                    LoadedMeshes.back().swap(cur_mesh);
                    meshMatNames.emplace_back(curMeshMatName);
                }
                #ifdef OBJL_CONSOLE_OUTPUT
                outputIndicator = outputEveryNth - 1;
                #endif
            }
            // Load Materials
            else if (*split_iter == "mtllib")
            {
                std::string pathtomat;
                std::string folder = "";
                if (Path.rfind('/') != std::string::npos)
                {
                    folder.assign(Path.begin(), Path.begin() + Path.rfind('/') + 1);
                }
                create_absolute_path(split_iter, folder, pathtomat);
                #ifdef OBJL_CONSOLE_OUTPUT
                std::cout << std::endl << "- find materials in: " << pathtomat << std::endl;
                #endif
                LoadMaterials(pathtomat);
            }
        }
    }

    #ifdef OBJL_CONSOLE_OUTPUT
    std::cout << std::endl;
    #endif

    // Deal with last mesh
    if (!cur_mesh.Indices.empty() && !cur_mesh.Vertices.empty())
    {
        LoadedMeshes.emplace_back();
        LoadedMeshes.back().swap(cur_mesh);
        meshMatNames.emplace_back(curMeshMatName);
    }

    file.close();

    // Set Materials for each Mesh
    for (size_t i = 0; i < meshMatNames.size(); ++i)
    {
        std::string const & matname = meshMatNames[i];
        auto iter = std::find_if(LoadedMaterials.begin(), LoadedMaterials.end(), [matname](Material const & m){return m.name == matname;});
        if (iter == LoadedMaterials.end())
        {
            std::cout << "Error, couldn't find Material" << std::endl;
        }
        else
        {
            LoadedMeshes[i].MeshMaterial = *iter;
        }
    }
    return !(LoadedMeshes.empty() && LoadedVertices == 0 && LoadedIndices == 0);
}

size_t Loader::VertexTriangluation(std::vector<uint32_t>& oIndices,
    std::vector<VertexCommon> const & iVerts,
    std::vector<uint64_t> & tVertInd,
    size_t offset)
{
    size_t iSize = oIndices.size() - offset;
    if (iSize < 3)
    {
        oIndices.erase(oIndices.begin() + offset, oIndices.end());
        return 0;
    }
    if (iSize == 3)
    {
        return 3;
    }
    tVertInd.assign(oIndices.begin() + offset, oIndices.end());
    oIndices.erase(oIndices.begin() + offset, oIndices.end());
    size_t oldSize = oIndices.size();
    do
    {
        for (size_t i = 0; i < tVertInd.size(); i++)
        {
            uint64_t iPrev = tVertInd[(i + tVertInd.size() - 1) % tVertInd.size()];
            uint64_t iCur = tVertInd[i];
            uint64_t iNext = tVertInd[(i + 1) % tVertInd.size()]; 
            if (tVertInd.size() == 3)
            {
                oIndices.push_back(iPrev);
                oIndices.push_back(iCur);
                oIndices.push_back(iNext);
                tVertInd.clear();
                break;
            }
            if (tVertInd.size() == 4)
            {
                oIndices.push_back(iPrev);
                oIndices.push_back(iCur);
                oIndices.push_back(iNext);
                oIndices.push_back(iNext);
                oIndices.push_back(tVertInd[(i + 2) % tVertInd.size()]);
                oIndices.push_back(iPrev);
                tVertInd.clear();
                break;
            }
            vec3f_t const & pPrev = iVerts[iPrev].Position;
            vec3f_t const & pCur = iVerts[iCur].Position;
            vec3f_t const & pNext = iVerts[iNext].Position;
            float dot = math::normdot(pPrev - pCur, pNext - pCur);
            if (dot <= 0 || dot >= 1)
                continue;

            bool inTri = false;
            for (size_t j = 0; j < iSize; j++)
            {
                if (j != iPrev && j != iCur && j != iNext && algorithm::inTriangle(iVerts[j].Position, pPrev, pCur, pNext))
                {
                    inTri = true;
                    break;
                }
            }
            if (inTri)
                continue;
            oIndices.push_back(iPrev);
            oIndices.push_back(iCur);
            oIndices.push_back(iNext);
            tVertInd.erase(tVertInd.begin() + i);
            i = std::numeric_limits<size_t>::max();
        }
    }
    while (!tVertInd.empty());
    return oIndices.size() - oldSize;
}

bool Loader::LoadMaterials(std::string path)
{
    std::cout << "load materials "<< path << std::endl;
    while (path.back() == '\13' || path.back() == '\0'){path.pop_back();}
    if (path.substr(path.size() - 4, path.size()) != ".mtl")
    {
        std::cout << "unexpected file ending: " << path.substr(path.size() - 4, path.size()) << std::endl;
        return false;
    }
    std::ifstream file(path);
    std::string folder = "";
    if (path.rfind('/') != std::string::npos)
    {
        folder.assign(path.begin(), path.begin() + path.rfind('/') + 1);
    }

    if (!file.is_open())
    {
        std::cout << "cant't open material file" << std::endl;
        return false;

    }
    Material *material = nullptr;

    std::string curline;
    auto split_iter = IO_UTIL::make_split_iterator("", [](char c){return c == ' ' || c == '\t';});

    while (std::getline(file, curline))
    {
        split_iter.str(curline);
        if (*split_iter == "newmtl")
        {
            LoadedMaterials.emplace_back();
            material = &LoadedMaterials.back();
            if (curline.size() > 7)
            {
                material->name = (++split_iter).remaining();
            }
            else
            {
                material->name = "none";
            }
        }
        else if (*split_iter == "Ka")       {read_vec(++split_iter, material->Ka);}
        else if (*split_iter == "Kd")       {read_vec(++split_iter, material->Kd);}
        else if (*split_iter == "Ks")       {read_vec(++split_iter, material->Ks);}
        else if (*split_iter == "Ns")       {(++split_iter).parse(material->Ns);}// Optical Density
        else if (*split_iter == "Ni")       {(++split_iter).parse(material->Ni);}// Dissolve
        else if (*split_iter == "d")        {(++split_iter).parse(material->d);}// Illumination
        else if (*split_iter == "illum")    {(++split_iter).parse(material->illum);}// Ambient Texture Map
        else if (*split_iter == "map_Ka")   {create_absolute_path(split_iter, folder, material->map_Ka);}   // Diffuse Texture Map
        else if (*split_iter == "map_Kd")   {create_absolute_path(split_iter, folder, material->map_Kd);}
        else if (*split_iter == "map_Ks")   {create_absolute_path(split_iter, folder, material->map_Ks);}// Specular Texture Map
        else if (*split_iter == "map_Ns")   {create_absolute_path(split_iter, folder, material->map_Ns);}// Specular Hightlight Map
        else if (*split_iter == "map_d")    {create_absolute_path(split_iter, folder, material->map_d);}// Alpha Texture Map
        else if (*split_iter == "map_Bump" || *split_iter == "map_bump" || *split_iter == "bump"){create_absolute_path(split_iter, folder, material->map_bump);}
    }
    std::cout << (LoadedMaterials.empty() ? "No materials found" :"Sucess") << std::endl;
    return !LoadedMaterials.empty();
}
}

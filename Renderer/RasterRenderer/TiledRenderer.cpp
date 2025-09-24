#include "RendererImplBase.h"
#include "CommonTraceCollection.h"
#include <algorithm>
#include <immintrin.h>

using namespace std;

namespace RasterRenderer
{
    class TiledRendererAlgorithm
    {
    private:

        // starter code uses 32x32 tiles, feel free to change
        static const int Log2TileSize = 5;
        static const int TileSize = 1 << Log2TileSize;

        // HINT: 
        // a compact representation of a frame buffer tile
        // Use if you wish (optional).  
        // (What other data-structures might be necessary?)
        // vector<FrameBuffer> frameBufferTiles; 

        // render target is grid of tiles: see SetFrameBuffer
        int gridWidth, gridHeight;
        FrameBuffer * frameBuffer;
        
        struct TiledTriangle {
            ProjectedTriangle triangle;
            int threadId;
            int triangleIndex;
        };
        
        vector<vector<TiledTriangle>> tileBins;
        vector<vector<TiledTriangle>> localTileBins[Cores];
    public:
        inline void Init()
        {

        }

        inline void Clear(const Vec4 & clearColor, bool color, bool depth)
        {
            frameBuffer->Clear(clearColor, color, depth);
        }

        inline void SetFrameBuffer(FrameBuffer * frameBuffer)
        {
            this->frameBuffer = frameBuffer;

            // compute number of necessary bins
            gridWidth = frameBuffer->GetWidth() >> Log2TileSize;
            gridHeight = frameBuffer->GetHeight() >> Log2TileSize;
            if (frameBuffer->GetWidth() & (TileSize - 1))
                gridWidth++;
            if (frameBuffer->GetHeight() & (TileSize - 1))
                gridHeight++;
            
            frameBuffer->Clear(Vec4(0,0,0,0), false, true);

            int numTiles = gridWidth * gridHeight;
            tileBins.assign(numTiles, vector<TiledTriangle>());
            
            for (int threadId = 0; threadId < Cores; threadId++) {
                localTileBins[threadId].assign(numTiles, vector<TiledTriangle>());
            }
        }

        inline void Finish()
        {
            // TODO: 
            // Finish() is called at the end of the frame. If it hasn't done so, your
            // implementation should flush local per-tile framebuffer contents to the 
            // global frame buffer (this->frameBuffer) here.
        }

        inline void BinTriangles(RenderState & state, ProjectedTriangleInput & input, int vertexOutputSize, int threadId)
        {
            auto & triangles = input.triangleBuffer[threadId];

            for (int i = 0; i < triangles.Count(); i++)
            {
                auto & tri = triangles[i];
                
                int bounds[4] = {
                    min(min(tri.X0, tri.X1), tri.X2) >> 4,
                    max(max(tri.X0, tri.X1), tri.X2) >> 4,
                    min(min(tri.Y0, tri.Y1), tri.Y2) >> 4,
                    max(max(tri.Y0, tri.Y1), tri.Y2) >> 4
                };
                
                bounds[0] = max(0, bounds[0]);
                bounds[1] = min(frameBuffer->GetWidth() - 1, bounds[1]);
                bounds[2] = max(0, bounds[2]);
                bounds[3] = min(frameBuffer->GetHeight() - 1, bounds[3]);
                
                int minX = bounds[0], maxX = bounds[1];
                int minY = bounds[2], maxY = bounds[3];
                
                int tileMinX = minX >> Log2TileSize;
                int tileMaxX = maxX >> Log2TileSize;
                int tileMinY = minY >> Log2TileSize;
                int tileMaxY = maxY >> Log2TileSize;
                
                for (int tileY = tileMinY; tileY <= tileMaxY; tileY++) {
                    for (int tileX = tileMinX; tileX <= tileMaxX; tileX++) {
                        int tileId = tileY * gridWidth + tileX;
                        if (tileId >= 0 && tileId < (int)localTileBins[threadId].size()) {
                            TiledTriangle tiledTri;
                            tiledTri.triangle = tri;
                            tiledTri.threadId = threadId;
                            tiledTri.triangleIndex = i;
                            localTileBins[threadId][tileId].push_back(tiledTri);
                        }
                    }
                }
            }
        }

        inline void ProcessBin(RenderState & state, ProjectedTriangleInput & input, int vertexOutputSize, int tileId)
        {
            if (tileId >= (int)tileBins.size()) return;
            
            int tileX = tileId % gridWidth;
            int tileY = tileId / gridWidth;
            int tilePixelX = tileX * TileSize;
            int tilePixelY = tileY * TileSize;
            int tilePixelW = min(TileSize, frameBuffer->GetWidth() - tilePixelX);
            int tilePixelH = min(TileSize, frameBuffer->GetHeight() - tilePixelY);
            
            static __m128i xOffset = _mm_set_epi32(24, 8, 24, 8);
            static __m128i yOffset = _mm_set_epi32(24, 24, 8, 8);
            
            for (const auto& tiledTri : tileBins[tileId]) {
                ProjectedTriangle tri = tiledTri.triangle;
                
                TriangleSIMD triSIMD;
                triSIMD.Load(tri);
                
                RasterizeTriangle(tilePixelX, tilePixelY, tilePixelW, tilePixelH, tri, triSIMD, 
                    [&](int qfx, int qfy, bool trivialAccept) {

                        __m128i coordX_center, coordY_center;
                        coordX_center = _mm_add_epi32(_mm_set1_epi32(qfx << 4), xOffset);
                        coordY_center = _mm_add_epi32(_mm_set1_epi32(qfy << 4), yOffset);
                        
                        
                        int coverageMask = trivialAccept ? 0xFFFF : triSIMD.TestQuadFragment(coordX_center, coordY_center);
                        
                        auto zValues = triSIMD.GetZ(coordX_center, coordY_center);
                        CORE_LIB_ALIGN_16(float zStore[4]);
                        _mm_store_ps(zStore, zValues);
                        
                        FragmentCoverageMask visibility;

                        __m128 currentZ = _mm_set_ps(
                            frameBuffer->GetZ(qfx + 1, qfy + 1, 0),
                            frameBuffer->GetZ(qfx, qfy + 1, 0),
                            frameBuffer->GetZ(qfx + 1, qfy, 0),
                            frameBuffer->GetZ(qfx, qfy, 0)
                        );
                        __m128 depthMask = _mm_cmplt_ps(zValues, currentZ);
                        int depthMaskInt = _mm_movemask_ps(depthMask);
                        
                        if ((coverageMask & 0x0008) && (depthMaskInt & 0x1)) {
                            visibility.SetBit(0);
                            frameBuffer->SetZ(qfx, qfy, 0, zStore[0]);
                        }
                        if ((coverageMask & 0x0080) && (depthMaskInt & 0x2)) {
                            visibility.SetBit(1);
                            frameBuffer->SetZ(qfx + 1, qfy, 0, zStore[1]);
                        }
                        if ((coverageMask & 0x0800) && (depthMaskInt & 0x4)) {
                            visibility.SetBit(2);
                            frameBuffer->SetZ(qfx, qfy + 1, 0, zStore[2]);
                        }
                        if ((coverageMask & 0x8000) && (depthMaskInt & 0x8)) {
                            visibility.SetBit(3);
                            frameBuffer->SetZ(qfx + 1, qfy + 1, 0, zStore[3]);
                        }

                        if (visibility.Any()) {

                            __m128 gamma, beta, alpha;
                            triSIMD.GetCoordinates(gamma, alpha, beta, coordX_center, coordY_center);

                            CORE_LIB_ALIGN_16(float shadeResult[16]);
                            ShadeFragment(state, shadeResult, beta, gamma, alpha, 
                                tiledTri.triangleIndex, tri.ConstantId, 
                                input.vertexOutputBuffer[tiledTri.threadId].Buffer(), 
                                vertexOutputSize, 
                                input.indexOutputBuffer[tiledTri.threadId].Buffer());

                            if (visibility.GetBit(0))
                                frameBuffer->SetPixel(qfx, qfy, 0, 
                                    Vec4(shadeResult[0], shadeResult[4], shadeResult[8], shadeResult[12]));
                            if (visibility.GetBit(1))
                                frameBuffer->SetPixel(qfx + 1, qfy, 0, 
                                    Vec4(shadeResult[1], shadeResult[5], shadeResult[9], shadeResult[13]));
                            if (visibility.GetBit(2))
                                frameBuffer->SetPixel(qfx, qfy + 1, 0, 
                                    Vec4(shadeResult[2], shadeResult[6], shadeResult[10], shadeResult[14]));
                            if (visibility.GetBit(3))
                                frameBuffer->SetPixel(qfx + 1, qfy + 1, 0, 
                                    Vec4(shadeResult[3], shadeResult[7], shadeResult[11], shadeResult[15]));
                        }
                    });
            }
        }


        inline void RenderProjectedBatch(RenderState & state, ProjectedTriangleInput & input, int vertexOutputSize)
        {
            for (auto& bin : tileBins) {
                bin.clear();
            }
            
            for (int threadId = 0; threadId < Cores; threadId++) {
                for (auto& bin : localTileBins[threadId]) {
                    bin.clear();
                }
            }
            
            // Pass 1:
            //
            // The renderer is structured so that input a set of triangle lists
            // (exactly one list of triangles for each core to process).
            // As shown in BinTriangles() above, each thread bins the triangles in
            // input.triangleBuffer[threadId]
            //
            // Below we create one task per core (i.e., one thread per
            // core).  That task should bin all the triangles in the
            // list it is provided into bins: via a call to BinTriangles
            Parallel::For(0, Cores, 1, [&](int threadId)
            {
                BinTriangles(state, input, vertexOutputSize, threadId);
            });

            for (int threadId = 0; threadId < Cores; threadId++) {
                for (int tileId = 0; tileId < (int)tileBins.size(); tileId++) {
                    tileBins[tileId].insert(tileBins[tileId].end(), 
                                          localTileBins[threadId][tileId].begin(), 
                                          localTileBins[threadId][tileId].end());
                }
            }

            // Pass 2:
            //
            // process all the tiles created in pass 1. Create one task per
            // tile (not one per core), and distribute all the tasks among the cores.  The
            // third parameter to the Parallel::For call is the work
            // distribution granularity.  Increasing its value might reduce
            // scheduling overhead (consecutive tiles go to the same core),
            // but could increase load imbalance.  (You can probably leave it
            // at one.)
            Parallel::For(0, gridWidth*gridHeight, 1, [&](int tileId)
            {
                ProcessBin(state, input, vertexOutputSize, tileId);
            });

        }
    };

    IRasterRenderer * CreateTiledRenderer()
    {
        return new RendererImplBase<TiledRendererAlgorithm>();
    }
}

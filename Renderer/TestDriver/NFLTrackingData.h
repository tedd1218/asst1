#ifndef NFL_TRACKING_DATA_H
#define NFL_TRACKING_DATA_H

#include "CoreLib/Basic.h"
#include "CoreLib/VectorMath.h"
#include <vector>
#include <map>

namespace RasterRenderer
{
    namespace NFL
    {
        using namespace CoreLib::Basic;
        using namespace VectorMath;

        struct PlayerPosition
        {
            int step;              // Frame number
            String team;           // "home" or "away"
            String position;       // Position abbreviation
            int jerseyNumber;
            float x;               // X position on field (yards)
            float y;               // Y position on field (yards)
            float speed;           // Speed (yards/second)
            float direction;       // Direction of movement (degrees)
            float orientation;     // Orientation (degrees)
            
            PlayerPosition()
                : step(0), jerseyNumber(0), x(0), y(0), speed(0), direction(0), orientation(0)
            {}
        };

        struct PlayData
        {
            String gameKey;
            String playId;
            String gamePlay;       // gameKey_playId format
            
            // Map from player ID to list of positions over time
            std::map<int, std::vector<PlayerPosition>> players;
            
            // Time steps (frame numbers) in the play
            std::vector<int> steps;
            
            PlayData() {}
        };

        class TrackingDataLoader
        {
        public:
            // Load tracking data from CSV file
            // Returns map from gamePlay (e.g., "58580_001136") to PlayData
            static std::map<String, PlayData> LoadFromCSV(const String& csvPath);
            
            // Get a specific play by gamePlay string
            static PlayData GetPlay(const String& csvPath, const String& gamePlay);
        };
    }
}

#endif


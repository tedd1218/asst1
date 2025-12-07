#include "NFLTrackingData.h"
#include "CoreLib/LibIO.h"
#include "CoreLib/LibString.h"
#include "CoreLib/TextIO.h"
#include <algorithm>

using namespace CoreLib::IO;

namespace RasterRenderer
{
    namespace NFL
    {
        std::map<String, PlayData> TrackingDataLoader::LoadFromCSV(const String& csvPath)
        {
            std::map<String, PlayData> plays;
            
            try
            {
                RefPtr<TextReader> reader = new StreamReader(new FileStream(csvPath, FileMode::Open));
                String line;
                bool firstLine = true;
                List<String> headers;
                
                int lineCount = 0;
                while (true)
                {
                    line = reader->ReadLine();
                    if (line.Length() == 0) break; // End of file
                    lineCount++;
                    if (lineCount % 100000 == 0)
                    {
                        printf("Processing line %d...\n", lineCount);
                    }
                    
                    // Parse CSV line
                    List<String> fields;
                    String current;
                    bool inQuotes = false;
                    
                    for (int i = 0; i < line.Length(); i++)
                    {
                        wchar_t c = line[i];
                        if (c == L'"')
                        {
                            inQuotes = !inQuotes;
                        }
                        else if (c == L',' && !inQuotes)
                        {
                            fields.Add(current);
                            current = L"";
                        }
                        else
                        {
                            current = current + String(c);
                        }
                    }
                    fields.Add(current);
                    
                    if (firstLine)
                    {
                        headers = fields;
                        firstLine = false;
                        continue;
                    }
                    
                    if (fields.Count() < headers.Count()) continue;
                    
                    // Find column indices
                    int gamePlayIdx = -1, gameKeyIdx = -1, playIdIdx = -1, playerIdIdx = -1;
                    int stepIdx = -1, teamIdx = -1, positionIdx = -1, jerseyIdx = -1;
                    int xIdx = -1, yIdx = -1, speedIdx = -1, directionIdx = -1, orientationIdx = -1;
                    
                    for (int i = 0; i < headers.Count(); i++)
                    {
                        String h = headers[i].ToLower();
                        if (h == L"game_play") gamePlayIdx = i;
                        else if (h == L"game_key") gameKeyIdx = i;
                        else if (h == L"play_id") playIdIdx = i;
                        else if (h == L"nfl_player_id") playerIdIdx = i;
                        else if (h == L"step") stepIdx = i;
                        else if (h == L"team") teamIdx = i;
                        else if (h == L"position") positionIdx = i;
                        else if (h == L"jersey_number") jerseyIdx = i;
                        else if (h == L"x_position") xIdx = i;
                        else if (h == L"y_position") yIdx = i;
                        else if (h == L"speed") speedIdx = i;
                        else if (h == L"direction") directionIdx = i;
                        else if (h == L"orientation") orientationIdx = i;
                    }
                    
                    if (gamePlayIdx < 0 || playerIdIdx < 0 || stepIdx < 0) continue;
                    
                    String gamePlay = fields[gamePlayIdx];
                    int playerId = StringToInt(fields[playerIdIdx]);
                    int step = StringToInt(fields[stepIdx]);
                    
                    // Get or create play
                    PlayData& play = plays[gamePlay];
                    if (play.gamePlay.Length() == 0)
                    {
                        play.gamePlay = gamePlay;
                        if (gameKeyIdx >= 0) play.gameKey = fields[gameKeyIdx];
                        if (playIdIdx >= 0) play.playId = fields[playIdIdx];
                    }
                    
                    // Create player position
                    PlayerPosition pos;
                    pos.step = step;
                    if (teamIdx >= 0) pos.team = fields[teamIdx];
                    if (positionIdx >= 0) pos.position = fields[positionIdx];
                    if (jerseyIdx >= 0) pos.jerseyNumber = StringToInt(fields[jerseyIdx]);
                    if (xIdx >= 0) pos.x = (float)StringToDouble(fields[xIdx]);
                    if (yIdx >= 0) pos.y = (float)StringToDouble(fields[yIdx]);
                    if (speedIdx >= 0) pos.speed = (float)StringToDouble(fields[speedIdx]);
                    if (directionIdx >= 0) pos.direction = (float)StringToDouble(fields[directionIdx]);
                    if (orientationIdx >= 0) pos.orientation = (float)StringToDouble(fields[orientationIdx]);
                    
                    // Add to player's position list
                    play.players[playerId].push_back(pos);
                    
                    // Track step
                    bool found = false;
                    for (int s : play.steps)
                    {
                        if (s == step)
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        play.steps.push_back(step);
                    }
                }
                
                // Sort steps for each play
                for (auto& pair : plays)
                {
                    std::sort(pair.second.steps.begin(), pair.second.steps.end());
                }
                
                printf("Loaded %d plays from tracking data\n", (int)plays.size());
            }
            catch (Exception& ex)
            {
                printf("Error loading tracking CSV: %s\n", ex.Message.ToMultiByteString());
            }
            
            return plays;
        }
        
        PlayData TrackingDataLoader::GetPlay(const String& csvPath, const String& gamePlay)
        {
            printf("  GetPlay: Starting...\n");
            fflush(stdout);
            PlayData playData;
            playData.gamePlay = gamePlay;
            
            try
            {
                printf("  GetPlay: Checking if file exists...\n");
                fflush(stdout);
                // Check if file exists first
                bool fileExists = false;
                try {
                    fileExists = File::Exists(csvPath);
                } catch (...) {
                    printf("  GetPlay: Exception in File::Exists\n");
                    fflush(stdout);
                    return playData;
                }
                
                if (!fileExists)
                {
                    printf("  GetPlay: Error: CSV file does not exist\n");
                    fflush(stdout);
                    return playData;
                }
                printf("  GetPlay: File exists, opening...\n");
                fflush(stdout);
                
                RefPtr<FileStream> fileStream;
                try {
                    fileStream = new FileStream(csvPath, FileMode::Open);
                } catch (Exception& ex) {
                    printf("  GetPlay: Exception creating FileStream: %s\n", ex.Message.ToMultiByteString());
                    fflush(stdout);
                    return playData;
                }
                printf("  GetPlay: FileStream created\n");
                fflush(stdout);
                
                RefPtr<TextReader> reader;
                try {
                    reader = new StreamReader(fileStream);
                } catch (Exception& ex) {
                    printf("  GetPlay: Exception creating StreamReader: %s\n", ex.Message.ToMultiByteString());
                    fflush(stdout);
                    return playData;
                }
                printf("  GetPlay: StreamReader created\n");
                fflush(stdout);
                String line;
                bool firstLine = true;
                List<String> headers;
                
                int lineCount = 0;
                int playLineCount = 0;
                printf("  GetPlay: Starting to read lines...\n");
                fflush(stdout);
                
                while (true)
                {
                    try {
                        line = reader->ReadLine();
                    } catch (Exception& ex) {
                        printf("  GetPlay: Exception reading line %d: %s\n", lineCount, ex.Message.ToMultiByteString());
                        fflush(stdout);
                        break;
                    } catch (...) {
                        printf("  GetPlay: Unknown exception reading line %d\n", lineCount);
                        fflush(stdout);
                        break;
                    }
                    
                    if (line.Length() == 0) {
                        printf("  GetPlay: End of file reached at line %d\n", lineCount);
                        fflush(stdout);
                        break; // End of file
                    }
                    
                    lineCount++;
                    if (lineCount == 1) {
                        printf("  GetPlay: Read first line successfully (length=%d)\n", line.Length());
                        fflush(stdout);
                    }
                    if (lineCount % 100000 == 0)
                    {
                        printf("  GetPlay: Processed %d lines, found %d play lines...\n", lineCount, playLineCount);
                        fflush(stdout);
                    }
                    
                    if (firstLine)
                    {
                        // Parse header
                        List<String> headerFields;
                        String current;
                        for (int i = 0; i < line.Length(); i++)
                        {
                            wchar_t c = line[i];
                            if (c == L',')
                            {
                                headerFields.Add(current);
                                current = L"";
                            }
                            else
                            {
                                current = current + String(c);
                            }
                        }
                        headerFields.Add(current);
                        headers = headerFields;
                        firstLine = false;
                        continue;
                    }
                    
                    // Quick check: does this line start with game_play?
                    bool isOurPlay = false;
                    if (line.Length() >= gamePlay.Length())
                    {
                        String lineStart = line.SubString(0, gamePlay.Length());
                        if (lineStart == gamePlay)
                        {
                            // Check if next char is comma (to avoid partial matches)
                            if (line.Length() > gamePlay.Length() && line[gamePlay.Length()] == L',')
                            {
                                isOurPlay = true;
                            }
                        }
                    }
                    
                    if (!isOurPlay) continue;
                    playLineCount++;
                    
                    // Parse CSV line for play
                    List<String> fields;
                    String current;
                    bool inQuotes = false;
                    
                    for (int i = 0; i < line.Length(); i++)
                    {
                        wchar_t c = line[i];
                        if (c == L'"')
                        {
                            inQuotes = !inQuotes;
                        }
                        else if (c == L',' && !inQuotes)
                        {
                            fields.Add(current);
                            current = L"";
                        }
                        else
                        {
                            current = current + String(c);
                        }
                    }
                    fields.Add(current);
                    
                    if (fields.Count() < headers.Count()) continue;
                    
                    // Find column indices
                    int gamePlayIdx = -1, gameKeyIdx = -1, playIdIdx = -1, playerIdIdx = -1;
                    int stepIdx = -1, teamIdx = -1, positionIdx = -1, jerseyIdx = -1;
                    int xIdx = -1, yIdx = -1, speedIdx = -1, directionIdx = -1, orientationIdx = -1;
                    
                    for (int i = 0; i < headers.Count(); i++)
                    {
                        String h = headers[i].ToLower();
                        if (h == L"game_play") gamePlayIdx = i;
                        else if (h == L"game_key") gameKeyIdx = i;
                        else if (h == L"play_id") playIdIdx = i;
                        else if (h == L"nfl_player_id") playerIdIdx = i;
                        else if (h == L"step") stepIdx = i;
                        else if (h == L"team") teamIdx = i;
                        else if (h == L"position") positionIdx = i;
                        else if (h == L"jersey_number") jerseyIdx = i;
                        else if (h == L"x_position") xIdx = i;
                        else if (h == L"y_position") yIdx = i;
                        else if (h == L"speed") speedIdx = i;
                        else if (h == L"direction") directionIdx = i;
                        else if (h == L"orientation") orientationIdx = i;
                    }
                    
                    if (gamePlayIdx < 0 || playerIdIdx < 0 || stepIdx < 0) continue;
                    
                    if (playData.gamePlay.Length() == 0)
                    {
                        playData.gamePlay = gamePlay;
                        if (gameKeyIdx >= 0) playData.gameKey = fields[gameKeyIdx];
                        if (playIdIdx >= 0) playData.playId = fields[playIdIdx];
                    }
                    
                    int playerId = StringToInt(fields[playerIdIdx]);
                    int step = StringToInt(fields[stepIdx]);
                    
                    // Create player position
                    PlayerPosition pos;
                    pos.step = step;
                    if (teamIdx >= 0) pos.team = fields[teamIdx];
                    if (positionIdx >= 0) pos.position = fields[positionIdx];
                    if (jerseyIdx >= 0) pos.jerseyNumber = StringToInt(fields[jerseyIdx]);
                    if (xIdx >= 0) pos.x = (float)StringToDouble(fields[xIdx]);
                    if (yIdx >= 0) pos.y = (float)StringToDouble(fields[yIdx]);
                    if (speedIdx >= 0) pos.speed = (float)StringToDouble(fields[speedIdx]);
                    if (directionIdx >= 0) pos.direction = (float)StringToDouble(fields[directionIdx]);
                    if (orientationIdx >= 0) pos.orientation = (float)StringToDouble(fields[orientationIdx]);
                    
                    // Add to player's position list
                    playData.players[playerId].push_back(pos);
                    
                    // Track step
                    bool found = false;
                    for (int s : playData.steps)
                    {
                        if (s == step)
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        playData.steps.push_back(step);
                    }
                }
                
                printf("  GetPlay: Finished reading, sorting steps...\n");
                fflush(stdout);
                
                // Sort steps
                std::sort(playData.steps.begin(), playData.steps.end());
                
                printf("  GetPlay: Steps sorted\n");
                fflush(stdout);
                
                printf("Loaded play %s: %d steps, %d players\n", 
                       gamePlay.ToMultiByteString(), 
                       (int)playData.steps.size(), 
                       (int)playData.players.size());
                fflush(stdout);
                
                // Explicitly close the reader and file stream before returning
                printf("  GetPlay: Closing file stream...\n");
                fflush(stdout);
                try {
                    reader->Close();
                } catch (...) {
                    // Ignore close errors
                }
                reader = nullptr;
                fileStream = nullptr;
                printf("  GetPlay: File stream closed\n");
                fflush(stdout);
            }
            catch (Exception& ex)
            {
                printf("  GetPlay: Exception in try block: %s\n", ex.Message.ToMultiByteString());
                fflush(stdout);
            }
            catch (std::exception& ex)
            {
                printf("  GetPlay: std::exception: %s\n", ex.what());
                fflush(stdout);
            }
            catch (...)
            {
                printf("  GetPlay: Unknown exception\n");
                fflush(stdout);
            }
            
            printf("  GetPlay: Returning playData\n");
            fflush(stdout);
            return playData;
        }
    }
}


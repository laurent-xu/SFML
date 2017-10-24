
////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"
#include <SFML/Graphics.hpp>
#include <vector>
#include <deque>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <utility>
#include <cmath>


namespace
{
    // Width and height of the application window
    const unsigned int windowWidth = 800;
    const unsigned int windowHeight = 600;

    // Resolution of the generated terrain
    const unsigned int resolutionX = 800;
    const unsigned int resolutionY = 600;

    // Thread pool parameters
    const unsigned int threadCount = 4;
    const unsigned int blockCount = 32;

    std::vector<sf::Thread*> threads;
    std::deque<std::pair<sf::VertexBuffer*, unsigned int> > workQueue;
    bool workPending = true;
    sf::Mutex workQueueMutex;

    // Terrain noise parameters
    const int perlinOctaves = 3;

    float perlinFrequency = 7.0f;
    float perlinFrequencyBase = 4.0f;

    // Terrain generation parameters
    float heightBase = 0.0f;
    float edgeFactor = 0.9f;
    float edgeDropoffExponent = 1.5f;

    float snowcapHeight = 0.6f;

    // Terrain lighting parameters
    float heightFactor = windowHeight / 2.0f;
    float heightFlatten = 3.0f;
    float lightFactor = 0.7f;
}


void threadFunction();
void generateTerrain(sf::VertexBuffer& vertexBuffer);


////////////////////////////////////////////////////////////
/// Entry point of application
///
/// \return Application exit code
///
////////////////////////////////////////////////////////////
int main()
{
    // Create the window of the application
    sf::RenderWindow window(sf::VideoMode(windowWidth, windowHeight), "SFML Island",
                            sf::Style::Titlebar | sf::Style::Close);
    window.setVerticalSyncEnabled(true);

    sf::Font font;
    if (!font.loadFromFile("resources/sansation.ttf"))
        return EXIT_FAILURE;

    bool prerequisitesSupported = sf::VertexBuffer::isAvailable() && sf::Shader::isAvailable();

    // Create all of our graphics resources
    sf::Text parametersText("", font);
    sf::Text statusText(prerequisitesSupported ? "Generating Terrain..." : "Shaders and/or Vertex Buffers Unsupported", font);
    sf::VertexBuffer terrain(sf::Triangles, sf::VertexBuffer::Static);
    sf::Shader terrainShader;
    sf::Texture terrainTexture;
    sf::RenderStates terrainStates;

    // Set up our graphics resources
    statusText.setCharacterSize(28);
    statusText.setFillColor(sf::Color::White);
    statusText.setPosition((windowWidth - statusText.getLocalBounds().width) / 2.f, (windowHeight - statusText.getLocalBounds().height) / 2.f);

    if (prerequisitesSupported)
    {
        statusText.setOutlineColor(sf::Color::Black);
        statusText.setOutlineThickness(2.0f);

        parametersText.setCharacterSize(14);
        parametersText.setFillColor(sf::Color::White);
        parametersText.setOutlineColor(sf::Color::Black);
        parametersText.setOutlineThickness(2.0f);
        parametersText.setPosition(5.0f, 5.0f);

        if (!terrainShader.loadFromFile("resources/terrain.vert", "resources/terrain.frag"))
            return EXIT_FAILURE;

        sf::Image terrainImage;
        terrainImage.create(2, 2, sf::Color::White);
        terrainTexture.loadFromImage(terrainImage);

        terrainStates.shader = &terrainShader;
        terrainStates.texture = &terrainTexture;

        // Start up our thread pool
        for (unsigned int i = 0; i < threadCount; i++)
        {
            threads.push_back(new sf::Thread(threadFunction));
            threads.back()->launch();
        }

        // Generate the initial terrain
        generateTerrain(terrain);
    }

    // Set up an array of pointers to our settings for arrow navigation
    float* settings[] =
    {
        &perlinFrequency,
        &perlinFrequencyBase,
        &heightBase,
        &edgeFactor,
        &edgeDropoffExponent,
        &snowcapHeight,
        &heightFactor,
        &heightFlatten,
        &lightFactor
    };

    const int settingCount = 9;
    int currentSetting = 0;

    std::ostringstream osstr;
    sf::Clock clock;

    while (window.isOpen())
    {
        // Handle events
        sf::Event event;
        while (window.pollEvent(event))
        {
            // Window closed or escape key pressed: exit
            if ((event.type == sf::Event::Closed) ||
               ((event.type == sf::Event::KeyPressed) && (event.key.code == sf::Keyboard::Escape)))
            {
                window.close();
                break;
            }

            // Arrow key pressed:
            if (prerequisitesSupported && (event.type == sf::Event::KeyPressed))
            {
                switch (event.key.code)
                {
                    case sf::Keyboard::Down:  currentSetting = (currentSetting + 1) % settingCount; break;
                    case sf::Keyboard::Up:    currentSetting = (currentSetting + settingCount - 1) % settingCount; break;
                    case sf::Keyboard::Left:  *settings[currentSetting] -= 0.1f; generateTerrain(terrain); break;
                    case sf::Keyboard::Right: *settings[currentSetting] += 0.1f; generateTerrain(terrain); break;
                    default: break;
                }
            }
        }

        // Clear, draw graphics objects and display
        window.clear();

        window.draw(statusText);

        if (prerequisitesSupported)
        {
            terrainShader.setUniform("lightFactor", lightFactor);
            window.draw(terrain, terrainStates);

            osstr.str("");
            osstr << "Frame: " << clock.restart().asMilliseconds() << "ms\n"
                  << "perlinOctaves: " << perlinOctaves << "\n"
                  << (settings[currentSetting] == &perlinFrequency ? "> " : "") << "perlinFrequency: " << perlinFrequency << "\n"
                  << (settings[currentSetting] == &perlinFrequencyBase ? "> " : "") << "perlinFrequencyBase: " << perlinFrequencyBase << "\n"
                  << (settings[currentSetting] == &heightBase ? "> " : "") << "heightBase: " << heightBase << "\n"
                  << (settings[currentSetting] == &edgeFactor ? "> " : "") << "edgeFactor: " << edgeFactor << "\n"
                  << (settings[currentSetting] == &edgeDropoffExponent ? "> " : "") << "edgeDropoffExponent: " << edgeDropoffExponent << "\n"
                  << (settings[currentSetting] == &snowcapHeight ? "> " : "") << "snowcapHeight: " << snowcapHeight << "\n"
                  << (settings[currentSetting] == &heightFactor ? "> " : "") << "heightFactor: " << heightFactor << "\n"
                  << (settings[currentSetting] == &heightFlatten ? "> " : "") << "heightFlatten: " << heightFlatten << "\n"
                  << (settings[currentSetting] == &lightFactor ? "> " : "") << "lightFactor: " << lightFactor << "\n";

            parametersText.setString(osstr.str());

            window.draw(parametersText);
        }

        // Display things on screen
        window.display();
    }

    // Shut down our thread pool
    {
        sf::Lock lock(workQueueMutex);
        workPending = false;
    }

    while (!threads.empty())
    {
        threads.back()->wait();
        delete threads.back();
        threads.pop_back();
    }

    return EXIT_SUCCESS;
}


////////////////////////////////////////////////////////////
/// Get the terrain elevation at the given coordinates.
///
////////////////////////////////////////////////////////////
float getElevation(float x, float y)
{
    x = x / resolutionX - 0.5f;
    y = y / resolutionY - 0.5f;

    float elevation = 0.0f;

    for (int i = 0; i < perlinOctaves; i++)
    {
        elevation += stb_perlin_noise3(
            x * perlinFrequency * std::pow(perlinFrequencyBase, i),
            y * perlinFrequency * std::pow(perlinFrequencyBase, i),
            0, 0, 0, 0
        ) * std::pow(perlinFrequencyBase, -i);
    }

    elevation = (elevation + 1.f) / 2.f;

    float distance = 2.0f * std::sqrt(x * x + y * y);
    elevation = (elevation + heightBase) * (1.0f - edgeFactor * std::pow(distance, edgeDropoffExponent));
    elevation = std::min(std::max(elevation, 0.0f), 1.0f);

    return elevation;
}


////////////////////////////////////////////////////////////
/// Get the terrain moisture at the given coordinates.
///
////////////////////////////////////////////////////////////
float getMoisture(float x, float y)
{
    x = x / resolutionX - 0.5f;
    y = y / resolutionY - 0.5f;

    float moisture = stb_perlin_noise3(
        x * 4.f + 0.5f,
        y * 4.f + 0.5f,
        0, 0, 0, 0
    );

    return (moisture + 1.f) / 2.f;
}


////////////////////////////////////////////////////////////
/// Get the lowlands terrain color for the given moisture.
///
////////////////////////////////////////////////////////////
sf::Color getLowlandsTerrainColor(float moisture)
{
    sf::Color color =
        moisture < 0.27f ? sf::Color(240, 240, 180) :
        moisture < 0.3f ? sf::Color(240 - 240 * (moisture - 0.27f) / 0.03f, 240 - 40 * (moisture - 0.27f) / 0.03f, 180 - 180 * (moisture - 0.27f) / 0.03f) :
        moisture < 0.4f ? sf::Color(0, 200, 0) :
        moisture < 0.48f ? sf::Color(0, 200 - 40 * (moisture - 0.4f) / 0.08f, 0) :
        moisture < 0.6f ? sf::Color(0, 160, 0) :
        moisture < 0.7f ? sf::Color(34 * (moisture - 0.6f) / 0.1f, 160 - 60 * (moisture - 0.6f) / 0.1f, 34 * (moisture - 0.6f) / 0.1f) :
        sf::Color(34, 100, 34);

    return color;
}


////////////////////////////////////////////////////////////
/// Get the highlands terrain color for the given elevation
/// and moisture.
///
////////////////////////////////////////////////////////////
sf::Color getHighlandsTerrainColor(float elevation, float moisture)
{
    sf::Color lowlandsColor = getLowlandsTerrainColor(moisture);

    sf::Color color =
        moisture < 0.6f ? sf::Color(112, 128, 144) :
        sf::Color(112 + 110 * (moisture - 0.6f) / 0.4f, 128 + 56 * (moisture - 0.6f) / 0.4f, 144 - 9 * (moisture - 0.6f) / 0.4f);

    float factor = std::min((elevation - 0.4f) / 0.1f, 1.f);

    color.r = lowlandsColor.r * (1.f - factor) + color.r * factor;
    color.g = lowlandsColor.g * (1.f - factor) + color.g * factor;
    color.b = lowlandsColor.b * (1.f - factor) + color.b * factor;

    return color;
}


////////////////////////////////////////////////////////////
/// Get the snowcap terrain color for the given elevation
/// and moisture.
///
////////////////////////////////////////////////////////////
sf::Color getSnowcapTerrainColor(float elevation, float moisture)
{
    sf::Color highlandsColor = getHighlandsTerrainColor(elevation, moisture);

    sf::Color color = sf::Color::White;

    float factor = std::min((elevation - snowcapHeight) / 0.05f, 1.f);

    color.r = highlandsColor.r * (1.f - factor) + color.r * factor;
    color.g = highlandsColor.g * (1.f - factor) + color.g * factor;
    color.b = highlandsColor.b * (1.f - factor) + color.b * factor;

    return color;
}


////////////////////////////////////////////////////////////
/// Get the terrain color for the given elevation and
/// moisture.
///
////////////////////////////////////////////////////////////
sf::Color getTerrainColor(float elevation, float moisture)
{
    sf::Color color =
        elevation < 0.11f ? sf::Color(0, 0, elevation / 0.11f * 74.f + 181.0f) :
        elevation < 0.14f ? sf::Color(std::pow((elevation - 0.11f) / 0.03f, 0.3f) * 48.f, std::pow((elevation - 0.11f) / 0.03f, 0.3f) * 48.f, 255) :
        elevation < 0.16f ? sf::Color((elevation - 0.14f) * 128.f / 0.02f + 48.f, (elevation - 0.14f) * 128.f / 0.02f + 48.f, 127.0f + (0.16f - elevation) * 128.f / 0.02f) :
        elevation < 0.17f ? sf::Color(240, 230, 140) :
        elevation < 0.4f ? getLowlandsTerrainColor(moisture) :
        elevation < snowcapHeight ? getHighlandsTerrainColor(elevation, moisture) :
        getSnowcapTerrainColor(elevation, moisture);

        return color;
}


////////////////////////////////////////////////////////////
/// Compute a compressed representation of the surface
/// normal based on the given coordinates, and the elevation
/// of the 4 adjacent neighbours.
///
////////////////////////////////////////////////////////////
sf::Vector2f computeNormal(int x, int y, float left, float right, float bottom, float top)
{
    sf::Vector3f deltaX(1, 0, (std::pow(right, heightFlatten) - std::pow(left, heightFlatten)) * heightFactor);
    sf::Vector3f deltaY(0, 1, (std::pow(top, heightFlatten) - std::pow(bottom, heightFlatten)) * heightFactor);

    sf::Vector3f crossProduct(
        deltaX.y * deltaY.z - deltaX.z * deltaY.y,
        deltaX.z * deltaY.x - deltaX.x * deltaY.z,
        deltaX.x * deltaY.y - deltaX.y * deltaY.x
    );

    // Scale cross product to make z component 1.0f so we can drop it
    crossProduct /= crossProduct.z;

    // Return "compressed" normal
    return sf::Vector2f(crossProduct.x, crossProduct.y);
}


////////////////////////////////////////////////////////////
/// Process a terrain generation work item. Use the vector
/// of vertices as scratch memory and upload the data to
/// the vertex buffer when done.
///
////////////////////////////////////////////////////////////
void processWorkItem(std::vector<sf::Vertex>& vertices, const std::pair<sf::VertexBuffer*, unsigned int>& workItem)
{
    unsigned int rowBlockSize = (resolutionY / blockCount) + 1;
    unsigned int rowStart = rowBlockSize * workItem.second;

    if (rowStart >= resolutionY)
        return;

    unsigned int rowEnd = std::min(rowStart + rowBlockSize, resolutionY);
    unsigned int rowCount = rowEnd - rowStart;

    const float scalingFactorX = static_cast<float>(windowWidth) / static_cast<float>(resolutionX);
    const float scalingFactorY = static_cast<float>(windowHeight) / static_cast<float>(resolutionY);

    for (int y = rowStart; y < rowEnd; y++)
    {
        for (int x = 0; x < resolutionX; x++)
        {
            int arrayIndexBase = ((y - rowStart) * resolutionX + x) * 6;

            // Top left corner (first triangle)
            if (x > 0)
            {
                vertices[arrayIndexBase + 0] = vertices[arrayIndexBase - 6 + 5];
            }
            else if (y > rowStart)
            {
                vertices[arrayIndexBase + 0] = vertices[arrayIndexBase - resolutionX * 6 + 1];
            }
            else
            {
                vertices[arrayIndexBase + 0].position = sf::Vector2f(x * scalingFactorX, y * scalingFactorY);
                vertices[arrayIndexBase + 0].color = getTerrainColor(getElevation(x, y), getMoisture(x, y));
                vertices[arrayIndexBase + 0].texCoords = computeNormal(x, y, getElevation(x - 1, y), getElevation(x + 1, y), getElevation(x, y + 1), getElevation(x, y - 1));
            }

            // Bottom left corner (first triangle)
            if (x > 0)
            {
                vertices[arrayIndexBase + 1] = vertices[arrayIndexBase - 6 + 2];
            }
            else
            {
                vertices[arrayIndexBase + 1].position = sf::Vector2f(x * scalingFactorX, (y + 1) * scalingFactorY);
                vertices[arrayIndexBase + 1].color = getTerrainColor(getElevation(x, y + 1), getMoisture(x, y + 1));
                vertices[arrayIndexBase + 1].texCoords = computeNormal(x, y + 1, getElevation(x - 1, y + 1), getElevation(x + 1, y + 1), getElevation(x, y + 2), getElevation(x, y));
            }

            // Bottom right corner (first triangle)
            vertices[arrayIndexBase + 2].position = sf::Vector2f((x + 1) * scalingFactorX, (y + 1) * scalingFactorY);
            vertices[arrayIndexBase + 2].color = getTerrainColor(getElevation(x + 1, y + 1), getMoisture(x + 1, y + 1));
            vertices[arrayIndexBase + 2].texCoords = computeNormal(x + 1, y + 1, getElevation(x, y + 1), getElevation(x + 2, y + 1), getElevation(x + 1, y + 2), getElevation(x + 1, y));

            // Top left corner (second triangle)
            vertices[arrayIndexBase + 3] = vertices[arrayIndexBase + 0];

            // Bottom right corner (second triangle)
            vertices[arrayIndexBase + 4] = vertices[arrayIndexBase + 2];

            // Top right corner (second triangle)
            if (y > rowStart)
            {
                vertices[arrayIndexBase + 5] = vertices[arrayIndexBase - resolutionX * 6 + 2];
            }
            else
            {
                vertices[arrayIndexBase + 5].position = sf::Vector2f((x + 1) * scalingFactorX, y * scalingFactorY);
                vertices[arrayIndexBase + 5].color = getTerrainColor(getElevation(x + 1, y), getMoisture(x + 1, y));
                vertices[arrayIndexBase + 5].texCoords = computeNormal(x + 1, y, getElevation(x, y), getElevation(x + 2, y), getElevation(x + 1, y + 1), getElevation(x + 1, y - 1));
            }
        }
    }

    workItem.first->update(&vertices[0], resolutionX * rowCount * 6, resolutionX * rowStart * 6);
}


////////////////////////////////////////////////////////////
/// Worker thread entry point. We use a thread pool to avoid
/// the heavy cost of constantly recreating and starting
/// new threads whenever we need to regenerate the terrain.
///
////////////////////////////////////////////////////////////
void threadFunction()
{
    sf::Context context;
    context.setActive(true);

    unsigned int rowBlockSize = (resolutionY / blockCount) + 1;

    std::pair<sf::VertexBuffer*, unsigned int> workItem(0, 0);

    std::vector<sf::Vertex> vertices;
    vertices.resize(resolutionX * rowBlockSize * 6);

    for (;;)
    {
        workItem.first = 0;

        {
            sf::Lock lock(workQueueMutex);

            if (!workPending)
                return;

            if (!workQueue.empty())
            {
                workItem = workQueue.front();
                workQueue.pop_front();
            }
        }

        if (!workItem.first)
        {
            sf::sleep(sf::milliseconds(10));

            continue;
        }

        processWorkItem(vertices, workItem);

        // Flush the context
        context.setActive(false);
        context.setActive(true);
    }
}


////////////////////////////////////////////////////////////
/// Terrain generation entry point. This queues up the
/// generation work items which the worker threads dequeue
/// and process.
///
////////////////////////////////////////////////////////////
void generateTerrain(sf::VertexBuffer& vertexBuffer)
{
    for (;;)
    {
        {
            sf::Lock lock(workQueueMutex);

            if(workQueue.empty())
                break;
        }

        sf::sleep(sf::milliseconds(10));
    }

    {
        sf::Lock lock(workQueueMutex);

        vertexBuffer.create(resolutionX * resolutionY * 6);

        for (unsigned int i = 0; i < blockCount; i++)
            workQueue.push_back(std::pair<sf::VertexBuffer*, unsigned int>(&vertexBuffer, i));
    }
}

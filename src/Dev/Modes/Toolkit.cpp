#include <ErrorHandler.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <TGUI/Gui.hpp>
#include <TGUI/Loading/Theme.hpp>
#include <TGUI/Widgets/Button.hpp>
#include <TGUI/Widgets/EditBox.hpp>
#include <TGUI/Widgets/Label.hpp>
#include <TGUI/Widgets/Panel.hpp>
#include <kaguya/kaguya.hpp>

#include <Bindings/Bindings.hpp>
#include <Modes/Toolkit.hpp>
#include <Modes/ToolkitContentBox.hpp>
#include <System/Loaders.hpp>
#include <System/Path.hpp>
#include <Utils/StringUtils.hpp>

namespace obe::Modes
{
    void startToolkitMode()
    {
        unsigned windowSize = sf::VideoMode::getDesktopMode().height / 1.5;
        bool continueToolkit = true;
        std::vector<std::string> commandHistory;
        unsigned commandHistoryIndex = 0;
        sf::RenderWindow window({ windowSize, windowSize }, "ObEngine Toolkit", sf::Style::None);
        sf::Color inputColor(255, 255, 255);

        sf::Font toolkitFont;
        toolkitFont.loadFromFile("Data/Fonts/weblysleekuil.ttf");

        sf::Font font;
        System::Path("Data/Fonts/weblysleekuil.ttf").load(System::Loaders::fontLoader, font);

        tgui::Gui gui(window);
        gui.setFont(font);
        tgui::Theme baseTheme;
        baseTheme.load("Data/GUI/obe.style");
        std::string currentMap = "";

        tgui::Panel::Ptr mainPanel = tgui::Panel::create();
        tgui::Panel::Ptr titlePanel = tgui::Panel::create();
        tgui::ToolkitContentBox::Ptr content = tgui::ToolkitContentBox::create();
        tgui::Scrollbar::Ptr scrollbar = tgui::Scrollbar::create();
        tgui::Label::Ptr titleLabel = tgui::Label::create();
        tgui::Button::Ptr closeButton = tgui::Button::create();
        tgui::EditBox::Ptr toolkitInput = tgui::EditBox::create();

        gui.add(mainPanel, "mainPanel");
        gui.add(titlePanel, "titlePanel");

        mainPanel->setRenderer(baseTheme.getRenderer("Panel"));
        mainPanel->setSize(window.getSize().x, window.getSize().y);
        mainPanel->setPosition("0", "0");

        titlePanel->setRenderer(baseTheme.getRenderer("Panel"));
        titlePanel->setSize("100%", "10%");
        titlePanel->setPosition("0", "0");

        content->setRenderer(baseTheme.getRenderer("ChatBox"));
        content->setSize("100%", "84%");
        content->setPosition("0", tgui::bindBottom(titlePanel));
        content->setLinesStartFromTop();
        mainPanel->add(content, "contentPanel");

        titleLabel->setRenderer(baseTheme.getRenderer("Label"));
        titleLabel->setText("ObEngine Toolkit");
        titleLabel->setTextSize(float(windowSize) * 0.06);
        titleLabel->setPosition("10", "10");
        titlePanel->add(titleLabel);

        closeButton->setRenderer(baseTheme.getRenderer("CloseButton"));
        closeButton->setSize("height", "50%");
        closeButton->setPosition("92%", "25%");
        closeButton->connect("pressed", [&window]() { window.close(); });
        titlePanel->add(closeButton);

        toolkitInput->setRenderer(baseTheme.getRenderer("TextBox"));
        toolkitInput->setSize("100%", "6%");
        toolkitInput->setPosition("0", tgui::bindBottom(content));
        mainPanel->add(toolkitInput);

        kaguya::State toolkitEngine;
        toolkitInput->connect("returnkeypressed",
            [&toolkitFont, &content, &toolkitInput, inputColor, &toolkitEngine, &commandHistory,
                &commandHistoryIndex, windowSize]() {
                sfe::RichText newtext(toolkitFont);
                newtext.setCharacterSize(windowSize * 0.025);
                std::string inputText = toolkitInput->getText().toAnsiString();
                newtext.pushString(">> " + inputText);
                newtext.pushFillColor(inputColor);
                content->addLine(newtext);
                toolkitEngine["evaluate"](inputText);
                toolkitInput->setText("");
                commandHistory.erase(std::remove_if(commandHistory.begin(), commandHistory.end(),
                                         [&inputText](const std::string& command) {
                                             return (command == inputText);
                                         }),
                    commandHistory.end());
                commandHistory.push_back(inputText);
                commandHistoryIndex = commandHistory.size();
            });

        toolkitEngine["This"] = &toolkitEngine;
        toolkitEngine.dofile("Lib/Internal/LuaCore.lua");
        toolkitEngine.dofile("Lib/Internal/ScriptInit.lua");
        Bindings::BindTree(&toolkitEngine);

        toolkitEngine["_term_set_input_color"]
            = kaguya::function([&inputColor](unsigned int r, unsigned int g, unsigned b) {
                  inputColor.r = r;
                  inputColor.g = g;
                  inputColor.b = b;
              });
        toolkitEngine["_term_display"] = kaguya::function(
            [&content, &toolkitFont, windowSize](
                const std::vector<sf::String>& strings, const std::vector<sf::Color>& colors) {
                sfe::RichText newtext(toolkitFont);
                newtext.setCharacterSize(windowSize * 0.025);
                for (int i = 0; i < strings.size(); i++)
                {
                    newtext.pushFillColor(colors.at(i));
                    newtext.pushString(strings.at(i));
                }
                content->addLine(newtext);
            });
        toolkitEngine["_term_clear"]
            = kaguya::function([&toolkitInput]() { toolkitInput->setText(""); });
        toolkitEngine["_term_write"] = kaguya::function([&toolkitInput](const std::string& string) {
            toolkitInput->setText(toolkitInput->getText() + string);
        });
        toolkitEngine["_term_last"] = kaguya::function([&toolkitInput]() {
            toolkitInput->setCaretPosition(toolkitInput->getText().getSize());
        });
        toolkitEngine["_term_get"] = kaguya::function(
            [&toolkitInput]() -> std::string { return toolkitInput->getText().toAnsiString(); });
        toolkitEngine["_term_close"]
            = kaguya::function([&continueToolkit]() { continueToolkit = false; });
        toolkitEngine.dofile("Lib/Toolkit/Toolkit.lua");

        sf::Vector2i grabbedOffset;
        bool grabbedWindow = false;

        gui.focusWidget(toolkitInput);

        while (window.isOpen() && continueToolkit)
        {
            sf::Event event;
            while (window.pollEvent(event))
            {
                if (event.type == sf::Event::Closed)
                    window.close();
                else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Tab)
                    toolkitEngine["autocomplete"](toolkitInput->getText().toAnsiString());
                else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Up)
                {
                    if (commandHistoryIndex > 0)
                    {
                        commandHistoryIndex--;
                        toolkitInput->setText(commandHistory[commandHistoryIndex]);
                    }
                }
                else if (event.type == sf::Event::KeyPressed
                    && event.key.code == sf::Keyboard::Down)
                {
                    if (commandHistoryIndex < commandHistory.size() - 1)
                    {
                        commandHistoryIndex++;
                        toolkitInput->setText(commandHistory[commandHistoryIndex]);
                    }
                }
                else if (event.type == sf::Event::MouseButtonPressed)
                {
                    if (sf::Mouse::getPosition().y - window.getPosition().y < 70
                        && sf::Mouse::getPosition().x - window.getPosition().x < 580)
                    {
                        if (event.mouseButton.button == sf::Mouse::Left)
                        {
                            grabbedOffset = window.getPosition() - sf::Mouse::getPosition();
                            grabbedWindow = true;
                        }
                    }
                }
                else if (event.type == sf::Event::MouseWheelMoved)
                {
                    content->mouseWheelScrolled(event.mouseWheel.delta, sf::Vector2f(0, 0));
                }
                else if (event.type == sf::Event::MouseButtonReleased)
                {
                    if (event.mouseButton.button == sf::Mouse::Left)
                        grabbedWindow = false;
                }
                else if (event.type == sf::Event::MouseMoved)
                {
                    if (grabbedWindow)
                        window.setPosition(sf::Mouse::getPosition() + grabbedOffset);
                }
                gui.handleEvent(event);
                if (event.type == sf::Event::TextEntered && event.text.unicode == 63)
                {
                    toolkitInput->setText(
                        Utils::String::replace(toolkitInput->getText().toAnsiString(), "?", ""));
                    toolkitEngine["printHelp"](toolkitInput->getText().toAnsiString());
                }
            }

            window.clear();
            gui.draw();
            window.display();
        }
    }
} // namespace obe::Modes

#include <Graphics/Canvas.hpp>
#include <Script/GlobalState.hpp>
#include "Graphics/ResourceManager.hpp"

namespace obe
{
    namespace Graphics
    {
        Element::Element(const std::string& id)
        {
            m_id = id;
            requires.push_back({"id", "\"\""});
        }

        Drawable::Drawable(const std::string& id) : Configurable(id), Element(id)
        {
            requires.push_back({"layer", "1"});
        }

        void Drawable::update()
        {
            Configurable::update();
            m_layer = m_tableWrapper["layer"];
        }

        Colorable::Colorable(const std::string& id) : Drawable(id), Configurable(id), Element(id)
        {
            requires.insert(requires.end(), 
            {
                {"color", "{ r = 255, g = 255, b = 255, a = 255}"}
            });
        }

        void Colorable::update()
        {
            Drawable::update();
            m_color = sf::Color(m_tableWrapper["color"]["r"],
                                m_tableWrapper["color"]["g"],
                                m_tableWrapper["color"]["b"],
                                m_tableWrapper["color"]["a"]);
        }

        Transformable::Transformable(const std::string& id) : Element(id), Configurable(id)
        {
            requires.insert(requires.end(),
            {
                {"x", "0"},
                {"y", "0"},
                {"width", "0"},
                {"height", "0"},
                {"rotation", "0"},
                {"origin", "{ translation = { x = 0, y = 0 }, rotation = { x = 0, y = 0 }}"}
            });
        }

        void Transformable::update()
        {
            Configurable::update();
            m_x = m_tableWrapper["x"];
            m_y = m_tableWrapper["y"];
            m_width = m_tableWrapper["width"];
            m_height = m_tableWrapper["height"];
            m_rotation = m_tableWrapper["rotation"];
            m_translationOriginX = m_tableWrapper["origin"]["translation"]["x"];
            m_translationOriginY = m_tableWrapper["origin"]["translation"]["y"];
            m_rotationOriginX = m_tableWrapper["origin"]["rotation"]["x"];
            m_rotationOriginY = m_tableWrapper["origin"]["rotation"]["y"];
        }

        Configurable::Configurable(const std::string& id) : Element(id)
        {
        }

        kaguya::LuaTable& Configurable::init(const kaguya::LuaTable& tableWrapper)
        {
            m_tableWrapper = tableWrapper;
            m_tableWrapper["id"] = m_id;
            return m_tableWrapper;
        }

        kaguya::LuaTable& Configurable::get()
        {
            return m_tableWrapper;
        }

        void Configurable::update()
        {
            Script::ScriptEngine["Core"]["Canvas"]["DefaultValues"](m_tableWrapper, requires);
        }

        CanvasElement::CanvasElement(const std::string& id) : Drawable(id), Configurable(id), Element(id)
        {
        }

        void CanvasElement::update()
        {
            Drawable::update();
        }

        Line::Line(const std::string& id) : CanvasElement(id), Drawable(id), Colorable(id), Configurable(id), Element(id)
        {
            requires.insert(requires.end(), 
            {
                {"x1", "0"},{"y1", "0"},
                {"x2", "0"},{"y2", "0"}
            });
        }

        void Line::draw(sf::RenderTexture& target) const
        {
            sf::Vertex line[] =
            {
                sf::Vertex(sf::Vector2f(m_x1, m_y1), m_color),
                sf::Vertex(sf::Vector2f(m_x2, m_y2), m_color)
            };
            target.draw(line, 2, sf::Lines);
        }

        void Line::update()
        {
            Colorable::update();

            m_x1 = m_tableWrapper["x1"];
            m_y1 = m_tableWrapper["y1"];
            m_x2 = m_tableWrapper["x2"];
            m_y2 = m_tableWrapper["y2"];
        }

        Rectangle::Rectangle(const std::string& id) : CanvasElement(id), Transformable(id), Drawable(id), Colorable(id), Configurable(id), Element(id)
        {
        }

        void Rectangle::draw(sf::RenderTexture& target) const
        {
            sf::RectangleShape rectangle(sf::Vector2f(m_width, m_height));
            rectangle.setPosition(m_x, m_y);
            rectangle.setFillColor(m_color);
            target.draw(rectangle);
        }

        void Rectangle::update()
        {
            Transformable::update();
            Colorable::update();
        }

        Text::Text(const std::string& id) : CanvasElement(id), Transformable(id), Drawable(id), Colorable(id), Configurable(id), Element(id)
        {
            requires.insert(requires.end(), 
            {
                {"text", "\"\""},{"font", "\"arial.ttf\""},{"size", "12"}
            });
        }

        void Text::draw(sf::RenderTexture& target) const
        {
            sf::Font font;
            font.loadFromFile(m_font);
            sf::Text text(m_text, font, m_characterSize);
            text.setPosition(m_x, m_y);

            target.draw(text);
        }

        void Text::update()
        {
            Transformable::update();
            Colorable::update();

            m_font = static_cast<const char*>(m_tableWrapper["font"]);
            m_text = static_cast<const char*>(m_tableWrapper["text"]);
            m_characterSize = m_tableWrapper["size"];
        }

        Circle::Circle(const std::string& id) : CanvasElement(id), Transformable(id), Drawable(id), Colorable(id), Configurable(id), Element(id)
        {
            requires.push_back({"radius", "1"});
        }

        void Circle::draw(sf::RenderTexture& target) const
        {
            sf::CircleShape circle;
            circle.setRadius(m_radius);
            circle.setFillColor(m_color);
            circle.setPosition(m_x, m_y);

            target.draw(circle);
        }

        void Circle::update()
        {
            Transformable::update();
            Colorable::update();

            m_radius = m_tableWrapper["radius"];
        }

        Sprite::Sprite(const std::string& id) : CanvasElement(id), Transformable(id), Drawable(id), Colorable(id), Configurable(id), Element(id)
        {
        }

        void Sprite::draw(sf::RenderTexture& target) const
        {
            sfe::ComplexSprite sprite;
            sprite.setTexture(*Graphics::ResourceManager::GetInstance()->getTexture(m_path));
            sprite.setTranslationOrigin(m_translationOriginX, m_translationOriginY);
            sprite.setRotationOrigin(m_rotationOriginX, m_rotationOriginY);
            sprite.setPosition(m_x, m_y);
            sprite.setRotation(m_rotation);
            sprite.setColor(m_color);
            //sprite.setScale(m_width, m_height);

            target.draw(sprite);
        }

        void Sprite::update()
        {
            Transformable::update();
            Colorable::update();

            std::string pathBuffer = m_tableWrapper["path"];
            m_path = pathBuffer;
        }

        Canvas::Canvas(unsigned int width, unsigned int height)
        {
            m_canvas.create(width, height);
        }

        Line* Canvas::line(const std::string& id)
        {
            Line* newLine = new Line(id);
            elements[id] = newLine;
            return newLine;
        }

        Rectangle* Canvas::rectangle(const std::string& id)
        {
            Rectangle* newRectangle = new Rectangle(id);
            elements[id] = newRectangle;
            return newRectangle;
        }

        Text* Canvas::text(const std::string& id)
        {
            Text* newText = new Text(id);
            elements[id] = newText;
            return newText;
        }

        Circle* Canvas::circle(const std::string& id)
        {
            Circle* newCircle = new Circle(id);
            elements[id] = newCircle;
            return newCircle;
        }

        Sprite* Canvas::sprite(const std::string& id)
        {
            Sprite* newSprite = new Sprite(id);
            elements[id] = newSprite;
            return newSprite;
        }

        kaguya::LuaTable& Canvas::get(std::string id)
        {
            return elements[id]->get();
        }

        void Canvas::setTarget(LevelSprite* target)
        {
            m_target = target;
        }

        void Canvas::render()
        {
            m_canvas.clear(sf::Color(0, 0, 0, 0));
            for (auto& element : elements)
            {
                element.second->update();
                element.second->draw(m_canvas);
            }
            m_canvas.display();
            m_target->setTexture(m_canvas.getTexture());
        }

        const sf::Texture& Canvas::getTexture() const
        {
            return m_canvas.getTexture();
        }
    }
}

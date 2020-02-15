#include <Bindings/Bindings.hpp>
#include <Scene/Scene.hpp>
#include <Script/GameObject.hpp>
#include <Script/GlobalState.hpp>
#include <Script/ViliLuaBridge.hpp>
#include <System/Loaders.hpp>
#include <Transform/UnitVector.hpp>
#include <Transform/Units.hpp>
#include <Triggers/Trigger.hpp>
#include <Triggers/TriggerDatabase.hpp>
#include <Utils/StringUtils.hpp>

#define GAMEOBJECTENV ScriptEngine["__ENVIRONMENTS"][m_envIndex]

namespace obe::Script
{
    /*kaguya::LuaTable GameObject::access(kaguya::State* lua) const
    {
        return (*m_objectScript)["Object"];
    }*/

    unsigned GameObject::getEnvIndex() const
    {
        return m_envIndex;
    }

    kaguya::LuaTable GameObject::access() const
    {
        return GAMEOBJECTENV["Object"];
    }

    kaguya::LuaFunction GameObject::getConstructor() const
    {
        return GAMEOBJECTENV["ObjectInit"];
    }

    vili::ViliParser GameObjectDatabase::allDefinitions;
    vili::ViliParser GameObjectDatabase::allRequires;
    vili::ComplexNode* GameObjectDatabase::GetRequirementsForGameObject(const std::string& type)
    {
        if (!allRequires.root().contains(type))
        {
            vili::ViliParser getGameObjectFile;
            System::Path("Data/GameObjects/")
                .add(type)
                .add(type + ".obj.vili")
                .load(System::Loaders::dataLoader, getGameObjectFile);
            if (getGameObjectFile->contains("Requires"))
            {
                vili::ComplexNode& requiresData
                    = getGameObjectFile.at<vili::ComplexNode>("Requires");
                getGameObjectFile->extractElement(
                    &getGameObjectFile.at<vili::ComplexNode>("Requires"));
                requiresData.setId(type);
                allRequires->pushComplexNode(&requiresData);
                return &requiresData;
            }
            return nullptr;
        }
        return &allRequires.at(type);
    }

    vili::ComplexNode* GameObjectDatabase::GetDefinitionForGameObject(const std::string& type)
    {
        if (!allDefinitions.root().contains(type))
        {
            vili::ViliParser getGameObjectFile;
            System::Path("Data/GameObjects/")
                .add(type)
                .add(type + ".obj.vili")
                .load(System::Loaders::dataLoader, getGameObjectFile);
            if (getGameObjectFile->contains(type))
            {
                vili::ComplexNode& definitionData = getGameObjectFile.at<vili::ComplexNode>(type);
                getGameObjectFile->extractElement(&getGameObjectFile.at<vili::ComplexNode>(type));
                definitionData.setId(type);
                allDefinitions->pushComplexNode(&definitionData);
                return &definitionData;
            }
            aube::ErrorHandler::Raise("ObEngine.Script.GameObjectDatabase.ObjectDefinitionNotFound",
                { { "objectType", type } });
            return nullptr;
        }
        return &allDefinitions.at(type);
    }

    void GameObjectDatabase::ApplyRequirements(GameObject* obj, vili::ComplexNode& requires)
    {
        for (vili::Node* currentRequirement : requires.getAll())
        {
            kaguya::LuaTable requireTable = ScriptEngine["__ENVIRONMENTS"][obj->getEnvIndex()]
                                                        ["LuaCore"]["ObjectInitInjectionTable"];
            DataBridge::dataToLua(requireTable, currentRequirement);
        }
    }

    void GameObjectDatabase::Clear()
    {
        allDefinitions->clear();
        allRequires->clear();
    }

    // GameObject
    std::vector<unsigned int> GameObject::AllEnvs;

    GameObject::GameObject(const std::string& type, const std::string& id)
        : Identifiable(id)
        , m_localTriggers(nullptr)
    {
        m_type = type;
    }

    void GameObject::initialize()
    {
        if (!m_active)
        {
            Debug::Log->debug("<GameObject> Initialising GameObject '{0}' ({1}) [Env={2}]", m_id,
                m_type, m_envIndex);
            m_active = true;
            if (m_hasScriptEngine)
            {
                GAMEOBJECTENV["__OBJECT_INIT"] = true;
                m_localTriggers->trigger("Init");
            }
        }
        else
            Debug::Log->warn("<GameObject> GameObject '{0}' ({1}) has already "
                             "been initialised",
                m_id, m_type);
    }

    GameObject::~GameObject()
    {
        Debug::Log->debug("<GameObject> Deleting GameObject '{0}' ({1})", m_id, m_type);
        this->deleteObject();
        AllEnvs.erase(std::remove_if(AllEnvs.begin(), AllEnvs.end(),
                          [this](const unsigned int& envIndex) { return envIndex == m_envIndex; }),
            AllEnvs.end());
        if (m_hasScriptEngine)
        {
            m_localTriggers.reset();
            Triggers::TriggerDatabase::GetInstance().removeNamespace(m_privateKey);
        }
    }

    void GameObject::sendInitArgFromLua(const std::string& argName, kaguya::LuaRef value) const
    {
        Debug::Log->debug("<GameObject> Sending Local.Init argument {0} to "
                          "GameObject {1} ({2}) (From Lua)",
            argName, m_id, m_type);
        m_localTriggers->pushParameterFromLua("Init", argName, value);
    }

    void GameObject::registerTrigger(
        std::weak_ptr<Triggers::Trigger> trg, const std::string& callbackName)
    {
        m_registeredTriggers.emplace_back(trg, callbackName);
    }

    void GameObject::loadGameObject(Scene::Scene& world, vili::ComplexNode& obj)
    {
        Debug::Log->debug("<GameObject> Loading GameObject '{0}' ({1})", m_id, m_type);
        // Script
        if (obj.contains(vili::NodeType::DataNode, "permanent"))
        {
            m_permanent = obj.getDataNode("permanent").get<bool>();
        }
        if (obj.contains(vili::NodeType::ComplexNode, "Script"))
        {
            m_hasScriptEngine = true;
            m_privateKey = Utils::String::getRandomKey(Utils::String::Alphabet, 1)
                + Utils::String::getRandomKey(Utils::String::Alphabet + Utils::String::Numbers, 11);
            Triggers::TriggerDatabase::GetInstance().createNamespace(m_privateKey);
            m_localTriggers.reset(
                Triggers::TriggerDatabase::GetInstance().createTriggerGroup(m_privateKey, "Local"),
                Triggers::TriggerGroupPtrRemover);

            m_envIndex = CreateNewEnvironment();
            Debug::Log->trace(
                "<GameObject> GameObject '{}' received Environment ID {}", m_id, m_envIndex);
            AllEnvs.push_back(m_envIndex);

            GAMEOBJECTENV["This"] = this;

            m_localTriggers->addTrigger("Init")->addTrigger("Delete");

            GAMEOBJECTENV["__OBJECT_TYPE"] = m_type;
            GAMEOBJECTENV["__OBJECT_ID"] = m_id;
            GAMEOBJECTENV["__OBJECT_INIT"] = false;
            GAMEOBJECTENV["Private"] = m_privateKey;

            executeFile(m_envIndex, System::Path("Lib/Internal/ObjectInit.lua").find());

            auto loadSource = [&](const std::string& path) {
                const std::string fullPath = System::Path(path).find();
                if (fullPath.empty())
                {
                    throw aube::ErrorHandler::Raise(
                        "obe.Script.GameObject.ScriptFileNotFound", { { "source", path } });
                }
                executeFile(m_envIndex, fullPath);
            };
            if (obj.at("Script").contains(vili::NodeType::DataNode, "source"))
            {
                loadSource(obj.at("Script").getDataNode("source").get<std::string>());
            }
            else if (obj.at("Script").contains(vili::NodeType::ArrayNode, "sources"))
            {
                const int scriptListSize = obj.at("Script").getArrayNode("sources").size();
                for (int i = 0; i < scriptListSize; i++)
                {
                    loadSource(obj.at("Script").getArrayNode("sources").get(i).get<std::string>());
                }
            }
        }
        // LevelSprite
        if (obj.contains(vili::NodeType::ComplexNode, "LevelSprite"))
        {
            m_sprite = world.createLevelSprite(m_id, false);
            m_objectNode.addChild(m_sprite);
            m_sprite->load(obj.at("LevelSprite"));
            if (m_hasScriptEngine)
                GAMEOBJECTENV["Object"]["LevelSprite"] = m_sprite;
            world.reorganizeLayers();
        }
        if (obj.contains(vili::NodeType::ComplexNode, "Animator"))
        {
            m_animator = std::make_unique<Animation::Animator>();
            const std::string animatorPath
                = obj.at("Animator").getDataNode("path").get<std::string>();
            if (m_sprite)
                m_animator->setTarget(*m_sprite);
            if (animatorPath != "")
            {
                m_animator->setPath(animatorPath);
                m_animator->loadAnimator();
            }
            if (obj.at("Animator").contains(vili::NodeType::DataNode, "default"))
            {
                m_animator->setKey(obj.at("Animator").getDataNode("default").get<std::string>());
            }
            if (m_hasScriptEngine)
                GAMEOBJECTENV["Object"]["Animation"] = m_animator.get();
        }
        // Collider
        if (obj.contains(vili::NodeType::ComplexNode, "Collider"))
        {
            m_collider = world.createCollider(m_id, false);
            m_objectNode.addChild(m_collider);
            m_collider->load(obj.at("Collider"));

            if (m_hasScriptEngine)
                GAMEOBJECTENV["Object"]["Collider"] = m_collider;
        }
    }

    void GameObject::update()
    {
        if (m_canUpdate)
        {
            if (m_active)
            {
                if (m_animator)
                {
                    if (m_animator->getKey() != "")
                        m_animator->update();
                    if (m_sprite)
                    {
                        m_sprite->setTexture(m_animator->getTexture());
                    }
                }
            }
            else
            {
                this->initialize();
            }
        }
    }

    std::string GameObject::getType() const
    {
        return m_type;
    }

    bool GameObject::doesHaveAnimator() const
    {
        return static_cast<bool>(m_animator);
    }

    bool GameObject::doesHaveCollider() const
    {
        return static_cast<bool>(m_collider);
    }

    bool GameObject::doesHaveLevelSprite() const
    {
        return static_cast<bool>(m_sprite);
    }

    bool GameObject::doesHaveScriptEngine() const
    {
        return m_hasScriptEngine;
    }

    bool GameObject::getUpdateState() const
    {
        return m_canUpdate;
    }

    void GameObject::setUpdateState(bool state)
    {
        m_canUpdate = state;
    }

    Graphics::LevelSprite* GameObject::getLevelSprite()
    {
        if (m_sprite)
            return m_sprite;
        throw aube::ErrorHandler::Raise(
            "ObEngine.Script.GameObject.NoLevelSprite", { { "id", m_id } });
    }

    Scene::SceneNode* GameObject::getSceneNode()
    {
        return &m_objectNode;
    }

    Collision::PolygonalCollider* GameObject::getCollider()
    {
        if (m_collider)
            return m_collider;
        throw aube::ErrorHandler::Raise(
            "ObEngine.Script.GameObject.NoCollider", { { "id", m_id } });
    }

    Animation::Animator* GameObject::getAnimator()
    {
        if (m_animator)
            return m_animator.get();
        throw aube::ErrorHandler::Raise(
            "ObEngine.Script.GameObject.NoAnimator", { { "id", m_id } });
    }

    void GameObject::useTrigger(const std::string& trNsp, const std::string& trGrp,
        const std::string& trName, const std::string& callAlias)
    {
        if (trName == "*")
        {
            std::vector<std::string> allTrg
                = Triggers::TriggerDatabase::GetInstance().getAllTriggersNameFromTriggerGroup(
                    trNsp, trGrp);
            for (const std::string& triggerName : allTrg)
            {
                this->useTrigger(trNsp, trGrp, triggerName,
                    (Utils::String::occurencesInString(callAlias, "*")
                            ? Utils::String::replace(callAlias, "*", triggerName)
                            : ""));
            }
        }
        else
        {
            bool triggerNotFound = true;
            for (auto& triggerPair : m_registeredTriggers)
            {
                if (triggerPair.first.lock()
                    == Triggers::TriggerDatabase::GetInstance()
                           .getTrigger(trNsp, trGrp, trName)
                           .lock())
                {
                    triggerNotFound = false;
                }
            }
            if (triggerNotFound)
            {
                const std::string callbackName
                    = (callAlias.empty()) ? trNsp + "." + trGrp + "." + trName : callAlias;
                this->registerTrigger(
                    Triggers::TriggerDatabase::GetInstance().getTrigger(trNsp, trGrp, trName),
                    callbackName);
                Triggers::TriggerDatabase::GetInstance()
                    .getTrigger(trNsp, trGrp, trName)
                    .lock()
                    ->registerEnvironment(m_envIndex, callbackName, &m_active);
            }
            else
            {
                const std::string callbackName
                    = (callAlias.empty()) ? trNsp + "." + trGrp + "." + trName : callAlias;
                Triggers::TriggerDatabase::GetInstance()
                    .getTrigger(trNsp, trGrp, trName)
                    .lock()
                    ->unregisterEnvironment(m_envIndex);
                Triggers::TriggerDatabase::GetInstance()
                    .getTrigger(trNsp, trGrp, trName)
                    .lock()
                    ->registerEnvironment(m_envIndex, callbackName, &m_active);
            }
        }
    }

    void GameObject::removeTrigger(
        const std::string& trNsp, const std::string& trGrp, const std::string& trName) const
    {
        Triggers::TriggerDatabase::GetInstance()
            .getTrigger(trNsp, trGrp, trName)
            .lock()
            ->unregisterEnvironment(m_envIndex);
    }

    void GameObject::exec(const std::string& query) const
    {
        executeString(m_envIndex, query);
    }

    void GameObject::deleteObject()
    {
        Debug::Log->debug("GameObject::deleteObject called for '{0}' ({1})", m_id, m_type);
        m_localTriggers->trigger("Delete");
        this->deletable = true;
        m_active = false;
        for (auto& triggerRef : m_registeredTriggers)
        {
            if (auto trigger = triggerRef.first.lock())
            {
                trigger->unregisterEnvironment(m_envIndex);
            }
        }
        // GAMEOBJECTENV = nullptr;
    }

    void GameObject::setPermanent(bool permanent)
    {
        m_permanent = permanent;
    }

    bool GameObject::isPermanent() const
    {
        return m_permanent;
    }
    void GameObject::setState(bool state)
    {
        m_active = state;
    }
} // namespace obe::Script
#pragma once

#include <Triggers/TriggerDelay.hpp>

namespace obe
{
    namespace Triggers
    {
        /**
         * \brief Class used to manage multiple Trigger
         * @Bind
         */
        class TriggerGroup
        {
        private:
            std::string m_name;
            std::string m_fromNsp;
            std::map<std::string, std::unique_ptr<Trigger>> m_triggerMap;
            std::vector<std::unique_ptr<TriggerDelay>> m_delayedTriggers;
            bool m_joinable = false;
            unsigned int m_references = 0;
            friend class TriggerDatabase;
        public:
            /**
             * \brief Creates a new TriggerGroup
             * \param triggerGroupNamespace Name of the Namespace the TriggerGroup is in
             * \param triggerGroupName Name of the TriggerGroup
             */
            explicit TriggerGroup(const std::string& triggerGroupNamespace, const std::string& triggerGroupName);
            /**
             * \brief Sets if the TriggerGroup is joinable or not
             * \param joinable true if the TriggerGroup should be joinable, false otherwise
             */
            void setJoinable(bool joinable);
            /**
             * \brief Get if the TriggerGroup is joinable or not
             * \return true if the TriggerGroup is joinable, false otherwise
             */
            bool isJoinable() const;
            /**
             * \brief Get a Trigger contained in the TriggerGroup
             * \param triggerName Name of the Trigger to get
             * \return A pointer to the Trigger if found (throws an error otherwise)
             */
            Trigger* getTrigger(const std::string& triggerName);
            /**
             * \brief Creates a new Trigger in the TriggerGroup
             * \param triggerName Name of the Trigger to create
             * \return Pointer to the TriggerGroup to chain calls
             */
            TriggerGroup* addTrigger(const std::string& triggerName);
            /**
             * \brief Delays activation of a Trigger
             * \param triggerName Name of the Trigger to delay
             * \param delay Time in ms used to delay the Trigger
             * \return Pointer to the TriggerGroup to chain calls
             */
            TriggerGroup* delayTriggerState(const std::string& triggerName, Time::TimeUnit delay);
            /**
             * \brief Enables a Trigger
             * \param triggerName Name of the Trigger to enable
             * \return Pointer to the TriggerGroup to chain calls
             */
            TriggerGroup* trigger(const std::string& triggerName);
            /**
             * \brief Get the current state of a Trigger
             * \param triggerName Name of the Trigger to get the state
             * \return true if the Trigger is enabled, false otherwise
             */
            bool getState(const std::string& triggerName);
            TriggerGroup* setPermanent(const std::string& triggerName, bool permanent);
            /**
             * \brief Pushes a Parameter to a Trigger
             * \tparam P Type of the Parameter
             * \param triggerName Name of the Trigger to push the parameter
             * \param parameterName Name of the parameter
             * \param parameter Value of the parameter
             */
            template <typename P>
            void pushParameter(const std::string& triggerName, const std::string& parameterName, P parameter);
            /**
             * \brief Pushes a Lua Parameter to a Trigger
             * \param triggerName Name of the Trigger to push the parameter
             * \param parameterName Name of the parameter
             * \param parameter Lua Value of the Parameter
             */
            void pushParameterFromLua(const std::string& triggerName, const std::string& parameterName, kaguya::LuaRef parameter);
            /**
             * \brief Get the name of all Trigger contained in the TriggerGroup
             * \return A std::vector of std::string containing the name of all Trigger contained in the TriggerGroup
             */
            std::vector<std::string> getAllTriggersName();
            /**
             * \brief Get all the Trigger contained in the TriggerGroup
             * \return A std::vector of Trigger pointers
             */
            std::vector<Trigger*> getAllTriggers();
            /**
             * \brief Get the name of the namespace the Trigger is in
             * \return A std::string containing the name of the namespace
             */
            std::string getNamespace() const;
            /**
             * \brief Get the name of the TriggerGroup
             * \return A std::string containing the name of the TriggerGroup
             */
            std::string getName() const;

            friend class TriggerGroupPtr;
        };

        /**
        * \brief Smart pointer for TriggerGroup
        */
        class TriggerGroupPtr
        {
        private:
            TriggerGroup* m_link = nullptr;
            static unsigned int amount;
            unsigned int m_id = 0;
            friend class TriggerDatabase;
        public:
            TriggerGroupPtr(TriggerGroup* link);
            TriggerGroupPtr& operator=(const TriggerGroupPtr& link);
            TriggerGroup* operator->() const;
            ~TriggerGroupPtr();
        };

        template <typename P>
        void TriggerGroup::pushParameter(const std::string& triggerName, const std::string& parameterName, P parameter)
        {
            getTrigger(triggerName)->pushParameter(parameterName, parameter);
        }
    }
}

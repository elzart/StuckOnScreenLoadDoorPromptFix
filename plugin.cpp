#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>

using namespace std::literals;

SKSEPluginInfo(
    .Version = { 1, 0, 0, 0 },
    .Name = "StuckOnScreenLoadDoorPromptFix"sv,
    .Author = ""sv,
    .SupportEmail = ""sv,
    .StructCompatibility = SKSE::StructCompatibility::Independent,
    .RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary
)

void SetupLog() {
    auto logsFolder = SKSE::log::log_directory();
    if (!logsFolder) SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");
    auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
    auto logFilePath = *logsFolder / std::format("{}.log", pluginName);
    auto fileLoggerPtr = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
    auto loggerPtr = std::make_shared<spdlog::logger>("log", std::move(fileLoggerPtr));
    spdlog::set_default_logger(std::move(loggerPtr));
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
}

namespace LoadDoorFix
{
    void ClearStuckLoadDoorPrompt()
    {
        auto* ui = RE::UI::GetSingleton();
        if (!ui) return;

        auto menu = ui->GetMenu(RE::HUDMenu::MENU_NAME);
        if (!menu || !menu->uiMovie) return;

        RE::GFxValue savedText;
        menu->uiMovie->GetVariable(&savedText, "_root.HUDMovieBaseInstance.SavedRolloverText");

        if (savedText.IsString()) {
            auto* str = savedText.GetString();
            if (str && str[0] != '\0') {
                menu->uiMovie->Invoke("_root.HUDMovieBaseInstance.SetLoadDoorInfo", nullptr, nullptr, 0);
                SKSE::log::info("Cleared stuck load door prompt");
            }
        }
    }

    class LocationChangeHandler final : public RE::BSTEventSink<RE::TESActorLocationChangeEvent>
    {
    public:
        static LocationChangeHandler* GetSingleton()
        {
            static LocationChangeHandler singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESActorLocationChangeEvent* a_event,
            RE::BSTEventSource<RE::TESActorLocationChangeEvent>*) override
        {
            if (!a_event || !a_event->actor) return RE::BSEventNotifyControl::kContinue;

            if (a_event->actor->IsPlayerRef()) {
                ClearStuckLoadDoorPrompt();
            }

            return RE::BSEventNotifyControl::kContinue;
        }

    private:
        LocationChangeHandler() = default;
        LocationChangeHandler(const LocationChangeHandler&) = delete;
        LocationChangeHandler(LocationChangeHandler&&) = delete;
        LocationChangeHandler& operator=(const LocationChangeHandler&) = delete;
        LocationChangeHandler& operator=(LocationChangeHandler&&) = delete;
    };
}

void OnSKSEMessage(SKSE::MessagingInterface::Message* a_msg)
{
    switch (a_msg->type) {
    case SKSE::MessagingInterface::kDataLoaded:
    {
        auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
        if (holder) {
            holder->AddEventSink(LoadDoorFix::LocationChangeHandler::GetSingleton());
            SKSE::log::info("Registered location change event handler");
        }
        break;
    }
    case SKSE::MessagingInterface::kPostLoadGame:
    case SKSE::MessagingInterface::kNewGame:
        LoadDoorFix::ClearStuckLoadDoorPrompt();
        break;
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SetupLog();

    auto* plugin = SKSE::PluginDeclaration::GetSingleton();
    SKSE::log::info("{} {} loading...", plugin->GetName(), plugin->GetVersion());

    SKSE::Init(skse);

    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(OnSKSEMessage)) {
        SKSE::log::error("Failed to register messaging listener");
        return false;
    }

    SKSE::log::info("{} loaded.", plugin->GetName());
    return true;
}

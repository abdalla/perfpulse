#include "core/i18n.hpp"

#include <windows.h>

#include <atomic>
#include <unordered_map>

namespace perfpulse::core {

namespace {

struct Entry {
    std::string_view key;
    std::string_view pt;
    std::string_view en;
};

constexpr Entry kEntries[] = {
    // Tray
    {"tray.status.paused", "Monitoramento pausado", "Monitoring paused"},
    {"tray.status.waiting", "Aguardando um jogo…", "Waiting for a game…"},
    {"tray.status.candidate", "Detectando: {0}", "Detecting: {0}"},
    {"tray.status.recording", "Gravando: {0} ({1})", "Recording: {0} ({1})"},
    {"tray.status.generating", "Gerando relatório…", "Generating report…"},
    {"tray.status.no_etw", "FPS indisponível: execute como administrador",
     "FPS unavailable: run as administrator"},
    {"tray.enable", "Monitoramento ativo", "Monitoring enabled"},
    {"tray.stop_now", "Encerrar gravação e gerar relatório", "Stop recording and build report"},
    {"tray.settings", "Configurações…", "Settings…"},
    {"tray.last_report", "Abrir último relatório", "Open last report"},
    {"tray.history", "Abrir histórico de partidas", "Open session history"},
    {"tray.open_folder", "Abrir pasta de relatórios", "Open reports folder"},
    {"tray.demo_report", "Gerar relatório de demonstração", "Generate demo report"},
    {"tray.exit", "Sair", "Exit"},
    // Notifications
    {"notify.report_ready.title", "Relatório pronto: {0}", "Report ready: {0}"},
    {"notify.report_ready.body", "{0} de jogo · {1} FPS médio · nota {2}. Clique para abrir.",
     "{0} played · {1} avg FPS · grade {2}. Click to open."},
    {"notify.session_short.title", "Partida muito curta", "Session too short"},
    {"notify.session_short.body", "{0}: menos de {1} s de dados, nenhum relatório gerado.",
     "{0}: less than {1} s of data, no report generated."},
    {"notify.recording.title", "PerfPulse gravando", "PerfPulse recording"},
    {"notify.recording.body", "Monitorando {0}. O relatório sai quando o jogo fechar.",
     "Monitoring {0}. The report is built when the game closes."},
    {"notify.report_failed.title", "Falha ao gerar relatório", "Failed to build report"},
    {"notify.already_running", "O PerfPulse já está em execução na bandeja.",
     "PerfPulse is already running in the tray."},
    // Settings window
    {"settings.title", "PerfPulse — Configurações", "PerfPulse — Settings"},
    {"settings.what_to_show", "O QUE MOSTRAR", "WHAT TO SHOW"},
    {"settings.what_to_show.hint",
     "Escolha o que aparece no relatório de cada partida. Uso de GPU/CPU, VRAM, RAM e consumo são sempre "
     "coletados para a análise de gargalo e energia. Hotspot, temperatura da VRAM, tensão e limitador dependem "
     "da placa. Temperatura e consumo da CPU precisam do PawnIO (ou HWiNFO/LibreHardwareMonitor abertos).",
     "Choose what appears in each session report. GPU/CPU usage, VRAM, RAM and power are always collected for "
     "the bottleneck and energy analysis. Hotspot, VRAM temperature, voltage and limiter depend on the card. "
     "CPU temperature and power need PawnIO (or HWiNFO/LibreHardwareMonitor running)."},
    {"settings.preset.fps_only", "SÓ FPS", "FPS ONLY"},
    {"settings.preset.default", "PADRÃO", "DEFAULT"},
    {"settings.preset.all", "TUDO", "ALL"},
    {"settings.general", "GERAL", "GENERAL"},
    {"settings.monitor", "Monitor do jogo", "Game monitor"},
    {"settings.monitor.any", "Qualquer monitor", "Any monitor"},
    {"settings.require_fullscreen", "Só tela cheia / sem bordas", "Fullscreen / borderless only"},
    {"settings.detection_delay", "Confirmar jogo após (s)", "Confirm game after (s)"},
    {"settings.min_session", "Partida mínima (s)", "Minimum session (s)"},
    {"settings.sample_interval", "Intervalo dos sensores", "Sensor interval"},
    {"settings.report_dir", "Pasta dos relatórios", "Reports folder"},
    {"settings.browse", "Procurar…", "Browse…"},
    {"settings.finish_action", "Ao terminar a partida", "When the session ends"},
    {"settings.finish.notify", "Notificar (clique abre)", "Notify (click opens)"},
    {"settings.finish.open", "Abrir o relatório", "Open the report"},
    {"settings.finish.silent", "Só salvar", "Save silently"},
    {"settings.language", "Idioma", "Language"},
    {"settings.language.auto", "Automático (Windows)", "Automatic (Windows)"},
    {"settings.start_with_windows", "Iniciar com o Windows", "Start with Windows"},
    {"settings.always_monitor", "Sempre monitorar (um .exe por linha)", "Always monitor (one .exe per line)"},
    {"settings.never_monitor", "Nunca monitorar (um .exe por linha)", "Never monitor (one .exe per line)"},
    {"settings.save", "Salvar", "Save"},
    {"settings.cancel", "Cancelar", "Cancel"},
    {"settings.sources", "Fontes detectadas", "Detected sources"},
    {"settings.autostart_failed", "Não foi possível configurar a inicialização com o Windows.",
     "Could not configure starting with Windows."},
    {"settings.language_restart", "O novo idioma vale a partir da próxima janela aberta.",
     "The new language applies to windows opened from now on."},
    // Groups
    {"group.fps", "FPS", "FPS"},
    {"group.gpu", "GPU", "GPU"},
    {"group.vram", "VRAM", "VRAM"},
    {"group.cpu", "CPU", "CPU"},
    {"group.ram", "RAM", "RAM"},
    {"group.other", "OUTROS", "OTHER"},
    // Metrics
    {"metric.fps", "FPS", "FPS"},
    {"metric.frametime", "Frametime (ms)", "Frametime (ms)"},
    {"metric.one_percent_low", "1% low", "1% low"},
    {"metric.frametime_graph", "Gráfico de frametime", "Frametime graph"},
    {"metric.gpu_usage", "Uso", "Usage"},
    {"metric.gpu_temperature", "Temperatura", "Temperature"},
    {"metric.gpu_hotspot", "Hotspot", "Hotspot"},
    {"metric.gpu_core_clock", "Clock do núcleo", "Core clock"},
    {"metric.gpu_memory_clock", "Clock da memória", "Memory clock"},
    {"metric.gpu_power", "Consumo", "Power"},
    {"metric.gpu_voltage", "Tensão do núcleo", "Core voltage"},
    {"metric.gpu_fan", "Ventoinha", "Fan"},
    {"metric.gpu_limiter", "Limitador (PerfCap)", "Limiter (PerfCap)"},
    {"metric.vram_used_gb", "Usada (GB)", "Used (GB)"},
    {"metric.vram_usage_percent", "Uso (%)", "Usage (%)"},
    {"metric.vram_temperature", "Temperatura", "Temperature"},
    {"metric.cpu_usage", "Uso total", "Total usage"},
    {"metric.cpu_busiest_core", "Núcleo mais ocupado", "Busiest core"},
    {"metric.cpu_temperature", "Temperatura", "Temperature"},
    {"metric.cpu_clock_average", "Clock médio", "Average clock"},
    {"metric.cpu_clock_max", "Clock máximo (núcleo)", "Max clock (core)"},
    {"metric.cpu_power", "Consumo", "Power"},
    {"metric.ram_used_gb", "Usada (GB)", "Used (GB)"},
    {"metric.ram_usage_percent", "Uso (%)", "Usage (%)"},
    {"metric.game_name", "Nome do jogo", "Game name"},
    {"metric.wall_clock", "Hora", "Time of day"},
    // Diagnostics / sources
    {"source.none", "indisponível", "unavailable"},
    {"source.etw_ok", "FPS: ETW ativo", "FPS: ETW active"},
    {"source.etw_denied", "FPS: sem permissão (admin)", "FPS: no permission (admin)"},
    {"source.etw_failed", "FPS: ETW falhou", "FPS: ETW failed"},
};

std::atomic<Language> gLanguage{Language::PortugueseBrazil};

const std::unordered_map<std::string_view, const Entry*>& entryIndex() {
    static const auto kIndex = [] {
        std::unordered_map<std::string_view, const Entry*> index;
        for (const auto& entry : kEntries) {
            index.emplace(entry.key, &entry);
        }
        return index;
    }();
    return kIndex;
}

}  // namespace

Language resolveLanguage(Language language) {
    if (language != Language::Auto) {
        return language;
    }
    const LANGID uiLanguage = GetUserDefaultUILanguage();
    return PRIMARYLANGID(uiLanguage) == LANG_PORTUGUESE ? Language::PortugueseBrazil : Language::English;
}

std::string_view languageTag(Language language) {
    return resolveLanguage(language) == Language::English ? "en" : "pt-BR";
}

void setLanguage(Language language) {
    gLanguage.store(resolveLanguage(language));
}

Language currentLanguage() {
    return gLanguage.load();
}

std::string_view tr(std::string_view key) {
    const auto& index = entryIndex();
    const auto it = index.find(key);
    if (it == index.end()) {
        return key;
    }
    return gLanguage.load() == Language::English ? it->second->en : it->second->pt;
}

std::string format(std::string_view pattern, std::initializer_list<std::string_view> args) {
    std::string result;
    result.reserve(pattern.size() + 32);
    for (size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] == '{' && i + 2 < pattern.size() && pattern[i + 2] == '}' && pattern[i + 1] >= '0' &&
            pattern[i + 1] <= '9') {
            const auto index = static_cast<size_t>(pattern[i + 1] - '0');
            if (index < args.size()) {
                result += *(args.begin() + index);
            }
            i += 2;
            continue;
        }
        result.push_back(pattern[i]);
    }
    return result;
}

}  // namespace perfpulse::core

package pt.orimbux.pad.raiox

/** Lista fixa de todos os testes do Raio-X, pela ordem do relatório. */
enum class Section(val title: String) {
    INPUTS("Entradas"),
    OUTPUTS("Saídas do comando"),
    AUDIO("Áudio, háptica e latência"),
}

data class TestDef(val id: String, val section: Section, val label: String, val auto: Boolean = false)

object RaioXTests {
    val all: List<TestDef> = listOf(
        // ---- Entradas (verificadas automaticamente) ----
        TestDef("sdl", Section.INPUTS, "SDL3 arrancou e abriu o comando", auto = true),
        TestDef("btn_cross", Section.INPUTS, "Botão ✕", auto = true),
        TestDef("btn_circle", Section.INPUTS, "Botão ○", auto = true),
        TestDef("btn_square", Section.INPUTS, "Botão □", auto = true),
        TestDef("btn_triangle", Section.INPUTS, "Botão △", auto = true),
        TestDef("btn_up", Section.INPUTS, "Seta ↑", auto = true),
        TestDef("btn_down", Section.INPUTS, "Seta ↓", auto = true),
        TestDef("btn_left", Section.INPUTS, "Seta ←", auto = true),
        TestDef("btn_right", Section.INPUTS, "Seta →", auto = true),
        TestDef("btn_l1", Section.INPUTS, "L1", auto = true),
        TestDef("btn_r1", Section.INPUTS, "R1", auto = true),
        TestDef("btn_l3", Section.INPUTS, "L3 (clique do stick esquerdo)", auto = true),
        TestDef("btn_r3", Section.INPUTS, "R3 (clique do stick direito)", auto = true),
        TestDef("btn_ps", Section.INPUTS, "PS", auto = true),
        TestDef("btn_create", Section.INPUTS, "Create", auto = true),
        TestDef("btn_options", Section.INPUTS, "Options", auto = true),
        TestDef("btn_mute", Section.INPUTS, "Mute", auto = true),
        TestDef("btn_touchpad", Section.INPUTS, "Clique do touchpad", auto = true),
        TestDef("stick_left", Section.INPUTS, "Stick esquerdo (4 extremos)", auto = true),
        TestDef("stick_right", Section.INPUTS, "Stick direito (4 extremos)", auto = true),
        TestDef("trig_l2", Section.INPUTS, "L2 analógico (meio curso + fundo)", auto = true),
        TestDef("trig_r2", Section.INPUTS, "R2 analógico (meio curso + fundo)", auto = true),
        TestDef("touch_1", Section.INPUTS, "Touchpad: 1 dedo", auto = true),
        TestDef("touch_2", Section.INPUTS, "Touchpad: 2 dedos ao mesmo tempo", auto = true),
        TestDef("gyro", Section.INPUTS, "Giroscópio", auto = true),
        TestDef("accel", Section.INPUTS, "Acelerómetro (gravidade ≈ 9,8 m/s²)", auto = true),
        TestDef("battery", Section.INPUTS, "Bateria do comando", auto = true),
        TestDef("report_rate", Section.INPUTS, "Taxa de reports (≥ 200 Hz)", auto = true),
        TestDef("reconnect", Section.INPUTS, "Religação automática (tirar e voltar a pôr o cabo)", auto = true),

        // ---- Saídas (confirmadas por ti) ----
        TestDef("rumble_left", Section.OUTPUTS, "Rumble esquerdo (forte)"),
        TestDef("rumble_right", Section.OUTPUTS, "Rumble direito (fino)"),
        TestDef("lightbar", Section.OUTPUTS, "Lightbar RGB"),
        TestDef("player_leds", Section.OUTPUTS, "5 player LEDs (cada um)"),
        TestDef("mute_led", Section.OUTPUTS, "Mute LED (aceso e a pulsar)"),
        TestDef("l2_feedback", Section.OUTPUTS, "L2: feedback (resistência)"),
        TestDef("l2_weapon", Section.OUTPUTS, "L2: weapon (clique)"),
        TestDef("l2_vibration", Section.OUTPUTS, "L2: vibração"),
        TestDef("l2_slope", Section.OUTPUTS, "L2: rampa progressiva (ECHO OUT)"),
        TestDef("l2_off", Section.OUTPUTS, "L2: off (volta a ficar livre)"),
        TestDef("r2_feedback", Section.OUTPUTS, "R2: feedback (resistência)"),
        TestDef("r2_weapon", Section.OUTPUTS, "R2: weapon (clique)"),
        TestDef("r2_vibration", Section.OUTPUTS, "R2: vibração"),
        TestDef("r2_rhythmic", Section.OUTPUTS, "R2: clique ritmado 1/16 a 172 BPM (BEAT ROLL)"),
        TestDef("r2_off", Section.OUTPUTS, "R2: off (volta a ficar livre)"),

        // ---- Áudio ----
        TestDef("haptic_audio", Section.AUDIO, "Háptica por áudio (canais 3–4)"),
        TestDef("ctrl_jack", Section.AUDIO, "Som no jack do comando (canais 1–2)"),
        TestDef("ctrl_speaker", Section.AUDIO, "Som no altifalante do comando"),
        TestDef("ctrl_mic", Section.AUDIO, "Gravação pelo microfone do comando"),
        TestDef("scn_a", Section.AUDIO, "Cenário A: música no telemóvel + háptica no comando"),
        TestDef("scn_b", Section.AUDIO, "Cenário B: música no jack + háptica (1 stream de 4 canais)"),
        TestDef("scn_a_hub", Section.AUDIO, "Cenário A através de hub USB-C com carregamento PD"),
        TestDef("scn_b_hub", Section.AUDIO, "Cenário B através de hub USB-C com carregamento PD"),
    )

    fun byId(id: String) = all.first { it.id == id }
}

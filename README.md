# Mainuan Welcome

Aplicação nativa de boas-vindas do Mainuan para KDE Plasma. A interface é Qt Quick/QML e a integração com o sistema é implementada em C++ seguro, sem Electron, Chromium, Node.js ou WebView.

## Arquitetura

- `src/services/`: tema e estilo visual, diagnósticos locais, Flatpak, instalador (`InstallService`) e single-instance.
- `src/models/`: modelos tipados para aplicativos, tutoriais e layouts.
- `qml/`: janela Kirigami e páginas Qt Quick Controls.
- `rootfs/usr/share/welcome/`: apenas assets locais herdados do protótipo.
- `rootfs/usr/libexec/mainuan-welcome/`: helper privilegiado executado via `pkexec`; `rootfs/usr/share/polkit-1/actions/`: ação polkit correspondente.
- `tests/fixtures/`: backends simulados (pkexec, helper, dpkg-query, flatpak) usados pelos testes.
- `docs/`: auditoria, decisões de compatibilidade e resultados de teste.

## Dependências

Build: CMake 3.24+, compilador C++17, Qt 6.5+ (`Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`, `Network`, `DBus`, `Multimedia`, `Test`) e KF6 Config. No Mainuan/Ubuntu, as dependências exatas estão em `debian/control`.

Runtime: Qt 6, Qt Quick Controls 2 e Kirigami 6 disponível no sistema KDE, `pkexec` (polkit) e um agente de autenticação. A instalação de antivírus, firewall e codecs usa APT e, portanto, requer Debian/Ubuntu; em outros sistemas ela aparece como indisponível. Flatpak, UFW, `kcmshell6` e o gerenciador de drivers são opcionais; a ausência deles é tratada na interface.

## Build e execução

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
./build/bin/mainuan-welcome
ctest --test-dir build --output-on-failure
```

Para instalar em um staging root:

```bash
cmake --install build --prefix "$PWD/stage"
```

O pacote instala o binário em `bin/`, o helper em `libexec/mainuan-welcome/`, a ação polkit em `share/polkit-1/actions/` (com o caminho do helper gerado pelo CMake), o desktop file em `share/applications/` e os metadados AppStream em `share/metainfo/`. Os assets são embutidos no recurso Qt para a interface funcionar offline.

## Pacote para o Mainuan

```bash
sudo apt-get install devscripts equivs
sudo mk-build-deps -i -r debian/control
dpkg-buildpackage -us -uc -b
sudo apt-get install ../mainuan-welcome_*_amd64.deb
```

O pacote instala também `/etc/xdg/autostart/org.mainuan.Welcome.desktop`, que abre o Welcome ao entrar na sessão; cada usuário pode desativar isso pela opção “Mostrar esta tela ao iniciar o sistema”.

## Aparência

“Estilo visual” aplica um tema global completo do Mainuan:

| Opção | Tema | Pacote Look-and-Feel (claro / escuro) | Papel de parede |
|---|---|---|---|
| Desfocado | Dream | `Dream-Light-Color-Global-6` / `Dream-Dark-Color-Global-6` | `01ciano.png` |
| Vítreo | Tahoe | `com.github.vinceliuice.MacTahoe-Light` / `-Dark` | `01ciano.png` |
| Sólido | Breeze | `org.kde.breeze.desktop` / `org.kde.breezedark.desktop` | `02cinza.png` |

- **Aplicação:** `plasma-apply-lookandfeel --apply <pacote>` (esquema de cores, tema Plasma, decoração, ícones, cursor), depois `plasma-apply-wallpaperimage`. Se o pacote citar ícones ou cursor que não estão instalados (caso do Tahoe), o Welcome usa os do Breeze via `plasma-changeicons`/`plasma-apply-cursortheme`. Tudo roda como o próprio usuário, sem root e sem reiniciar o Plasma.
- **Detecção:** `[KDE] LookAndFeelPackage` em `kdeglobals` e, quando ausente, `[Theme] name` em `plasmarc`, ambos lidos com KConfig pela cascata do KDE (os temas globais gravam em `~/.config/kdedefaults/`). Mudanças feitas fora do Welcome aparecem na hora (`KConfigWatcher`).
- **Tema do sistema** alterna a variante clara/escura do estilo atual; **Cor de destaque** usa `plasma-apply-colorscheme --accent-color` e grava `AccentColor`, que o Plasma mantém ao trocar de estilo.
- Os ids e arquivos são uma lista fechada em `src/services/VisualStyle.cpp`; o QML só envia `blur`, `glass` ou `solid`.

Diagnóstico: `journalctl --user --since today | grep mainuan.welcome` mostra cada estilo aplicado e detectado, e `kreadconfig6 --file kdeglobals --group KDE --key LookAndFeelPackage` dentro da sessão Plasma.

## Instalador

`InstallService` instala apenas perfis fechados (`antivirus`, `firewall`, `codecs` e `app:<id Flatpak permitido>`). Pacotes Debian são instalados pelo helper via `pkexec`, que recebe somente o nome do perfil; aplicativos vêm do Flathub. O progresso exibido vem do `APT::Status-Fd` e das linhas de progresso do Flatpak. Detalhes em `docs/appearance-and-installer-plan.md`.

Para testar a interface sem alterar o sistema, compile com `-DMAINUAN_DEVELOPER_MODE=ON` e aponte os backends para `tests/fixtures/` com `MAINUAN_DEV_PKEXEC`, `MAINUAN_DEV_HELPER`, `MAINUAN_DEV_DPKG_QUERY`, `MAINUAN_DEV_APT_GET` e `MAINUAN_DEV_FLATPAK` (estado em `MOCK_STATE_DIR`, cenário em `MOCK_SCENARIO`). Builds normais ignoram essas variáveis.

## Compatibilidade

O código usa baseline Qt 6.5 e APIs públicas estáveis. O ambiente de desenvolvimento desta auditoria executa Plasma 6.7.4 em Wayland; Plasma 6.6 Wayland é o alvo obrigatório, mas ainda requer validação em uma instalação real 6.6. Veja `docs/11-compatibilidade-plasma.md`.

## Contribuição

Consulte `docs/` antes de alterar integração de sistema, privilégios ou layouts. Não adicione telemetria, dependências online ou comandos construídos por concatenação.

PerfPulse {VERSION}
https://github.com/abdalla/perfpulse

== English ==

Lightweight game performance recorder for Windows. No installation needed.

1. Extract this folder anywhere (e.g. C:\Tools\PerfPulse).
2. Run PerfPulse.exe and accept the administrator prompt.
   If Windows shows "Windows protected your PC", click "More info" ->
   "Run anyway" (the exe is not code-signed).
3. Play. When the game closes, the report is saved to
   Documents\PerfPulse\Reports.

Requirements: Windows 10 21H2+ or Windows 11, x64.

CPU temperature and power need the PawnIO driver (https://pawnio.eu or
"winget install namazso.PawnIO"). Without it everything else still works.
Keep the modules\ folder next to PerfPulse.exe.

== Português ==

Gravador de desempenho de jogos para Windows. Não precisa instalar.

1. Extraia esta pasta onde quiser (ex.: C:\Tools\PerfPulse).
2. Rode o PerfPulse.exe e aceite o pedido de administrador.
   Se o Windows mostrar "O Windows protegeu o computador", clique em
   "Mais informações" -> "Executar assim mesmo" (o exe não é assinado).
3. Jogue. Quando o jogo fechar, o relatório vai para
   Documentos\PerfPulse\Reports.

Requisitos: Windows 10 21H2+ ou Windows 11, x64.

Temperatura e consumo da CPU precisam do driver PawnIO (https://pawnio.eu ou
"winget install namazso.PawnIO"). Sem ele, todo o resto funciona.
Mantenha a pasta modules\ ao lado do PerfPulse.exe.

== Licenses ==

PerfPulse: MIT (LICENSE.txt).
modules\*.bin: PawnIO.Modules, LGPL-2.1 (modules\LICENSE-PawnIO.Modules.txt),
source code at https://github.com/namazso/PawnIO.Modules

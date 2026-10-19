#include "annotation_remarks.h"

const std::string ST_HTML_ANNOTATION::m_c8Annotation = R"(<h3>Legend</h3>
<div class="centered">
<table>
<tbody>
<tr class="column-entry-bold">
<td>Marking</td>
<td>Description</td>
<td>Include in statistics</td>
</tr>
<tr class="light-row">
<td>E</td>
<td>Code has been executed during coverage testing</td>
<td>Yes</td>
</tr>
<tr class="light-row">
<td>AE</td>
<td>Code has been executed, but its execution was not detected by the code coverage tool</td>
<td>Yes</td>
</tr>
<tr class="light-row">
<td>A0</td>
<td>Line without executable code</td>
<td>No</td>
</tr>
<tr class="light-row">
<td>A1</td>
<td>Code executed only in PC simulation mode or under different conditional compilation settings</td>
<td>No</td>
</tr>
<tr class="light-row">
<td>A2</td>
<td>Line with code that should never be executed (defensive programming)</td>
<td>Yes</td>
</tr>
<tr class="light-row">
<td>A3</td>
<td>Fault insertion code (not used during normal operation)</td>
<td>Yes</td>
</tr>
<tr class="light-row">
<td>A4</td>
<td>Code execution depends on the sensor type and is not used in the current configuration</td>
<td>Yes</td>
</tr>
<tr class="light-row">
<td>A5</td>
<td>Code not used (implemented for future use)</td>
<td>Yes</td>
</tr>
<tr class="light-row">
<td>A6</td>
<td>Code used only in combination with CPU modem</td>
<td>Yes</td>
</tr>
<tr class="light-row">
<td>A7</td>
<td>Code executed only in the event of a hardware failure</td>
<td>Yes</td>
</tr>
<tr class="light-row">
<td>A8</td>
<td>Platform code not used by this device</td>
<td>Yes</td>
</tr>
<tr class="light-row">
<td>A9</td>
<td>Code for hardware testing used only in production</td>
<td>Yes</td>
</tr>
<tr class="light-row">
<td>A10</td>
<td>Code execution depends on the hardware variant and is not used in the current configuration</td>
<td>Yes</td>
</tr>
</tbody>
</table>
</div>
)";
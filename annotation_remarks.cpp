#include "annotation_remarks.h"

const std::string ST_HTML_ANNOTATION::m_c8Annotation = R"(<p><h3>Legend</h3></p>
<div class="centered">
<table>
<tbody>
<tr>
<td class="column-entry-bold">Marking</td>
<td class="column-entry-bold">Description</td>
<td class="column-entry-bold">Include in statistics</td>
</tr>
<tr>
<td class="light-row">E</td>
<td class="light-row">Code has been executed during coverage test</td>
<td class="light-row">Yes</td>
</tr>
<tr>
<td class="light-row">AE</td>
<td class="light-row">Code has been executed but not detected by code coverage tool</td>
<td class="light-row">Yes</td>
</tr>
<tr>
<td class="light-row">A0</td>
<td class="light-row">Line without executable code</td>
<td class="light-row">No</td>
</tr>
<tr>
<td class="light-row">A1</td>
<td class="light-row">Line with code executed only in PC simulation mode or other conditional compiling not enabled</td>
<td class="light-row">No</td>
</tr>
<tr>
<td class="light-row">A2</td>
<td class="light-row">Line with code that should never be executed (defensive programming)</td>
<td class="light-row">yes</td>
</tr>
<tr>
<td class="light-row">A3</td>
<td class="light-row">Fault insertion code (not used during normal operation)</td>
<td class="light-row">yes</td>
</tr>
<tr>
<td class="light-row">A4</td>
<td class="light-row">Line with code only used for clamp on sensors / other sensors</td>
<td class="light-row">yes</td>
</tr>
<tr>
<td class="light-row">A5</td>
<td class="light-row">Code not used (implemented for future use)</td>
<td class="light-row">yes</td>
</tr>
<tr>
<td class="light-row">A6</td>
<td class="light-row">Code only used in combination with CPU Modem</td>
<td class="light-row">yes</td>
</tr>
<tr>
<td class="light-row">A7</td>
<td class="light-row">Code only used in case of Hardware failure</td>
<td class="light-row">yes</td>
</tr>
<tr>
<td class="light-row">A8</td>
<td class="light-row">Platform code not used by this device</td>
<td class="light-row">yes</td>
</tr>
<tr>
<td class="light-row">A9</td>
<td class="light-row">Code for hardware testing  used only in production</td>
<td class="light-row">yes</td>
</tr>
<tr>
<td class="light-row">A10</td>
<td class="light-row">Code dependent on hardware component (not used with actual components) </td>
<td class="light-row">yes</td>
</tr>
</tbody>
</table>
</div>
)";
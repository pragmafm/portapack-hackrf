/*
 * Copyright (C) 2015 Jared Boone, ShareBrained Technology, Inc.
 *
 * This file is part of PortaPack.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include "proc_wideband_spectrum.hpp"

#include "event_m4.hpp"

#include "i2s.hpp"
using namespace lpc43xx;

#include "dsp_fft.hpp"

#include <cstdint>
#include <cstddef>

#include <array>

/*
Employ window-presum technique with a window constructed to act like a bin filter.
See: http://www.embedded.com/design/real-time-and-performance/4007611/DSP-Tricks-Building-a-practical-spectrum-analyzer

20MHz / 256 bins = 78.125kHz/bin

Taps made with IPython Notebook "specan_lpf", 26041.666667Hz pass, 53400Hz stop, 1024 taps, Remez, gain of 4 * 128.
*/
static constexpr std::array<float, 1024> window_wideband_bin_lpf { {
	 0.0000000000f, -0.1215729938f, -0.1213842597f, -0.1201684429f,
	-0.1201208291f, -0.1190024264f, -0.1190858794f, -0.1180910084f,
	-0.1182902140f, -0.1173947314f, -0.1177379144f, -0.1169320615f,
	-0.1174165122f, -0.1166760717f, -0.1173065305f, -0.1166353753f,
	-0.1174295604f, -0.1168115435f, -0.1177737061f, -0.1171681501f,
	-0.1183154633f, -0.1176885372f, -0.1190734477f, -0.1183823726f,
	-0.1200564042f, -0.1191925052f, -0.1212624937f, -0.1200347844f,
	-0.1227782673f, -0.1205369109f, -0.1247875900f, -0.1173893915f,
	-0.1356269751f, -0.1333820215f, -0.1230013683f, -0.1270419996f,
	-0.1248521391f, -0.1280766855f, -0.1275474455f, -0.1303677621f,
	-0.1305094750f, -0.1330985791f, -0.1335694857f, -0.1359864220f,
	-0.1366453783f, -0.1389458751f, -0.1397338876f, -0.1419434716f,
	-0.1428068116f, -0.1449481895f, -0.1458627832f, -0.1479788256f,
	-0.1489403301f, -0.1510623954f, -0.1520476681f, -0.1541795914f,
	-0.1551676484f, -0.1573540028f, -0.1583282171f, -0.1606274956f,
	-0.1614806858f, -0.1640690747f, -0.1645983125f, -0.1683593655f,
	-0.1685811576f, -0.1666149935f, -0.1723006308f, -0.1738005522f,
	-0.1764397529f, -0.1778294249f, -0.1798377067f, -0.1812660021f,
	-0.1831168886f, -0.1846305866f, -0.1864679413f, -0.1881030278f,
	-0.1899836720f, -0.1917311189f, -0.1936526632f, -0.1954904285f,
	-0.1974621775f, -0.1993785604f, -0.2013983100f, -0.2033652690f,
	-0.2054038199f, -0.2073919547f, -0.2094317510f, -0.2114475228f,
	-0.2134958020f, -0.2155244502f, -0.2175598198f, -0.2195723764f,
	-0.2216212397f, -0.2236245172f, -0.2256330226f, -0.2276718013f,
	-0.2285011851f, -0.2330266272f, -0.2341760732f, -0.2355468158f,
	-0.2372897729f, -0.2390892017f, -0.2410984360f, -0.2430630996f,
	-0.2451131271f, -0.2470901019f, -0.2491104380f, -0.2510271295f,
	-0.2529788379f, -0.2548171418f, -0.2567089643f, -0.2584896769f,
	-0.2603277216f, -0.2620527778f, -0.2638390865f, -0.2655133466f,
	-0.2672727018f, -0.2689155628f, -0.2706716369f, -0.2722769142f,
	-0.2740080396f, -0.2755608442f, -0.2772761254f, -0.2787902978f,
	-0.2804988036f, -0.2818896819f, -0.2836812630f, -0.2847616987f,
	-0.2872351631f, -0.2879411883f, -0.2887415908f, -0.2908411467f,
	-0.2922042842f, -0.2938671020f, -0.2950580040f, -0.2964174081f,
	-0.2974931002f, -0.2987161065f, -0.2997260603f, -0.3008800902f,
	-0.3018593574f, -0.3029682798f, -0.3039122280f, -0.3049525345f,
	-0.3058372184f, -0.3068011429f, -0.3076169401f, -0.3085060392f,
	-0.3092353383f, -0.3100296179f, -0.3106447420f, -0.3113288653f,
	-0.3118458639f, -0.3124439347f, -0.3128663068f, -0.3133679403f,
	-0.3136545108f, -0.3141297440f, -0.3142523940f, -0.3147549076f,
	-0.3146688330f, -0.3145152697f, -0.3154945639f, -0.3152392795f,
	-0.3152146050f, -0.3149481827f, -0.3148580017f, -0.3146624743f,
	-0.3145480407f, -0.3143048413f, -0.3140959719f, -0.3137462882f,
	-0.3133944699f, -0.3129048624f, -0.3123958369f, -0.3117751598f,
	-0.3111336729f, -0.3103913733f, -0.3096257208f, -0.3087558210f,
	-0.3078644089f, -0.3068771119f, -0.3058756301f, -0.3047953307f,
	-0.3036791840f, -0.3024750370f, -0.3012206975f, -0.2998832522f,
	-0.2985327732f, -0.2970457316f, -0.2955408715f, -0.2939578995f,
	-0.2921271926f, -0.2909536921f, -0.2886979015f, -0.2867034021f,
	-0.2848865889f, -0.2829509809f, -0.2809996598f, -0.2788599641f,
	-0.2766682382f, -0.2743418063f, -0.2719823809f, -0.2695102609f,
	-0.2670290887f, -0.2644425265f, -0.2618581518f, -0.2591571117f,
	-0.2564433319f, -0.2536057399f, -0.2507463340f, -0.2477656128f,
	-0.2447669078f, -0.2416375134f, -0.2384901545f, -0.2351846242f,
	-0.2318685842f, -0.2284043787f, -0.2249483716f, -0.2213504338f,
	-0.2177326536f, -0.2139529315f, -0.2102567274f, -0.2062425510f,
	-0.2025752799f, -0.1982312400f, -0.1941135048f, -0.1902676621f,
	-0.1859484086f, -0.1816310071f, -0.1771140890f, -0.1726183100f,
	-0.1680332327f, -0.1634326051f, -0.1587317239f, -0.1539912234f,
	-0.1491346512f, -0.1442198838f, -0.1391845207f, -0.1340828639f,
	-0.1288821834f, -0.1236191798f, -0.1182710743f, -0.1128619755f,
	-0.1073604731f, -0.1018001799f, -0.0961440884f, -0.0904454283f,
	-0.0846659246f, -0.0788312843f, -0.0729015436f, -0.0668947833f,
	-0.0607956180f, -0.0546891309f, -0.0483829023f, -0.0421512920f,
	-0.0356596108f, -0.0291537434f, -0.0229218816f, -0.0160240942f,
	-0.0093228281f, -0.0025108764f,  0.0042491810f,  0.0111402133f,
	 0.0180789645f,  0.0251689168f,  0.0322938011f,  0.0395537898f,
	 0.0468355725f,  0.0542297395f,  0.0616418371f,  0.0691492555f,
	 0.0766873597f,  0.0843282945f,  0.0920097856f,  0.0998030288f,
	 0.1076300783f,  0.1155633999f,  0.1235258532f,  0.1315923511f,
	 0.1397136271f,  0.1479358113f,  0.1562017822f,  0.1645454321f,
	 0.1729007855f,  0.1814067004f,  0.1898938500f,  0.1984767185f,
	 0.2071598962f,  0.2156750628f,  0.2246834377f,  0.2334738679f,
	 0.2422305215f,  0.2511740186f,  0.2601619588f,  0.2692982077f,
	 0.2784363297f,  0.2876515447f,  0.2968698668f,  0.3061706020f,
	 0.3154820195f,  0.3248945791f,  0.3343234494f,  0.3438662338f,
	 0.3534259170f,  0.3630855246f,  0.3727544495f,  0.3825072994f,
	 0.3922658126f,  0.4021151299f,  0.4119710813f,  0.4219304098f,
	 0.4318755419f,  0.4419066050f,  0.4519266790f,  0.4620411273f,
	 0.4721924196f,  0.4824099358f,  0.4925918161f,  0.5029672026f,
	 0.5131676220f,  0.5237112702f,  0.5340474666f,  0.5443687487f,
	 0.5549596923f,  0.5655037792f,  0.5760846623f,  0.5866412762f,
	 0.5972195405f,  0.6078470214f,  0.6185164530f,  0.6292346029f,
	 0.6399890869f,  0.6507775481f,  0.6615869909f,  0.6724215836f,
	 0.6832601692f,  0.6941326055f,  0.7050115342f,  0.7159351783f,
	 0.7268750405f,  0.7378519124f,  0.7488402816f,  0.7598478405f,
	 0.7708626189f,  0.7819213638f,  0.7929898181f,  0.8041032024f,
	 0.8151863856f,  0.8262675846f,  0.8374347030f,  0.8485124885f,
	 0.8596901081f,  0.8708298046f,  0.8818521053f,  0.8932043400f,
	 0.9043106868f,  0.9154712945f,  0.9265928796f,  0.9377943260f,
	 0.9489884395f,  0.9602246825f,  0.9714051258f,  0.9826151929f,
	 0.9937649659f,  1.0049460392f,  1.0160814715f,  1.0272496293f,
	 1.0383880993f,  1.0495571434f,  1.0606883842f,  1.0718420204f,
	 1.0829385916f,  1.0940532343f,  1.1051116703f,  1.1161955290f,
	 1.1272425119f,  1.1382946085f,  1.1492896227f,  1.1602787038f,
	 1.1712107201f,  1.1822261399f,  1.1931082102f,  1.2040321660f,
	 1.2149338949f,  1.2257216453f,  1.2366886195f,  1.2474232220f,
	 1.2581260885f,  1.2689166363f,  1.2796266048f,  1.2903413723f,
	 1.3009452557f,  1.3115347597f,  1.3220588330f,  1.3325885128f,
	 1.3430577723f,  1.3535351287f,  1.3639358829f,  1.3743362159f,
	 1.3846448962f,  1.3949381321f,  1.4051428918f,  1.4153304829f,
	 1.4254416597f,  1.4355455123f,  1.4455694200f,  1.4555829383f,
	 1.4654880955f,  1.4753745521f,  1.4851755647f,  1.4949664411f,
	 1.5047009642f,  1.5143471956f,  1.5238981621f,  1.5335015901f,
	 1.5428771970f,  1.5523880267f,  1.5616442576f,  1.5708923706f,
	 1.5801889794f,  1.5893352932f,  1.5983995178f,  1.6073964215f,
	 1.6163275268f,  1.6252361387f,  1.6340558312f,  1.6428196478f,
	 1.6514803963f,  1.6600728279f,  1.6685645243f,  1.6769969948f,
	 1.6853322111f,  1.6936257655f,  1.7018206388f,  1.7099724793f,
	 1.7180180271f,  1.7260006052f,  1.7338708023f,  1.7416709937f,
	 1.7493721167f,  1.7570307435f,  1.7645736334f,  1.7720587141f,
	 1.7793864240f,  1.7866595518f,  1.7939083209f,  1.8009680396f,
	 1.8080175914f,  1.8149423807f,  1.8217244084f,  1.8285934275f,
	 1.8351580541f,  1.8417084040f,  1.8481458306f,  1.8545405038f,
	 1.8608040839f,  1.8669819454f,  1.8730100097f,  1.8789794347f,
	 1.8848148272f,  1.8906028239f,  1.8962629246f,  1.9018629597f,
	 1.9073312666f,  1.9127216689f,  1.9179684698f,  1.9231368617f,
	 1.9281575220f,  1.9331147367f,  1.9379346294f,  1.9426980945f,
	 1.9473253537f,  1.9518608547f,  1.9562473102f,  1.9605563249f,
	 1.9647377083f,  1.9689130417f,  1.9728201948f,  1.9767204935f,
	 1.9804869777f,  1.9840965410f,  1.9877044695f,  1.9910612418f,
	 1.9943519434f,  1.9976117863f,  2.0006898142f,  2.0036817820f,
	 2.0065068911f,  2.0092612533f,  2.0118899336f,  2.0144381201f,
	 2.0168403950f,  2.0191500613f,  2.0212978024f,  2.0233571150f,
	 2.0252586877f,  2.0270797012f,  2.0287613011f,  2.0303632435f,
	 2.0318318976f,  2.0332139460f,  2.0344458593f,  2.0355811762f,
	 2.0365507956f,  2.0374411193f,  2.0382032761f,  2.0388791314f,
	 2.0394289467f,  2.0397935314f,  2.0400654417f,  2.0403149729f,
	 2.0402865682f,  2.0403149729f,  2.0400654417f,  2.0397935314f,
	 2.0394289467f,  2.0388791314f,  2.0382032761f,  2.0374411193f,
	 2.0365507956f,  2.0355811762f,  2.0344458593f,  2.0332139460f,
	 2.0318318976f,  2.0303632435f,  2.0287613011f,  2.0270797012f,
	 2.0252586877f,  2.0233571150f,  2.0212978024f,  2.0191500613f,
	 2.0168403950f,  2.0144381201f,  2.0118899336f,  2.0092612533f,
	 2.0065068911f,  2.0036817820f,  2.0006898142f,  1.9976117863f,
	 1.9943519434f,  1.9910612418f,  1.9877044695f,  1.9840965410f,
	 1.9804869777f,  1.9767204935f,  1.9728201948f,  1.9689130417f,
	 1.9647377083f,  1.9605563249f,  1.9562473102f,  1.9518608547f,
	 1.9473253537f,  1.9426980945f,  1.9379346294f,  1.9331147367f,
	 1.9281575220f,  1.9231368617f,  1.9179684698f,  1.9127216689f,
	 1.9073312666f,  1.9018629597f,  1.8962629246f,  1.8906028239f,
	 1.8848148272f,  1.8789794347f,  1.8730100097f,  1.8669819454f,
	 1.8608040839f,  1.8545405038f,  1.8481458306f,  1.8417084040f,
	 1.8351580541f,  1.8285934275f,  1.8217244084f,  1.8149423807f,
	 1.8080175914f,  1.8009680396f,  1.7939083209f,  1.7866595518f,
	 1.7793864240f,  1.7720587141f,  1.7645736334f,  1.7570307435f,
	 1.7493721167f,  1.7416709937f,  1.7338708023f,  1.7260006052f,
	 1.7180180271f,  1.7099724793f,  1.7018206388f,  1.6936257655f,
	 1.6853322111f,  1.6769969948f,  1.6685645243f,  1.6600728279f,
	 1.6514803963f,  1.6428196478f,  1.6340558312f,  1.6252361387f,
	 1.6163275268f,  1.6073964215f,  1.5983995178f,  1.5893352932f,
	 1.5801889794f,  1.5708923706f,  1.5616442576f,  1.5523880267f,
	 1.5428771970f,  1.5335015901f,  1.5238981621f,  1.5143471956f,
	 1.5047009642f,  1.4949664411f,  1.4851755647f,  1.4753745521f,
	 1.4654880955f,  1.4555829383f,  1.4455694200f,  1.4355455123f,
	 1.4254416597f,  1.4153304829f,  1.4051428918f,  1.3949381321f,
	 1.3846448962f,  1.3743362159f,  1.3639358829f,  1.3535351287f,
	 1.3430577723f,  1.3325885128f,  1.3220588330f,  1.3115347597f,
	 1.3009452557f,  1.2903413723f,  1.2796266048f,  1.2689166363f,
	 1.2581260885f,  1.2474232220f,  1.2366886195f,  1.2257216453f,
	 1.2149338949f,  1.2040321660f,  1.1931082102f,  1.1822261399f,
	 1.1712107201f,  1.1602787038f,  1.1492896227f,  1.1382946085f,
	 1.1272425119f,  1.1161955290f,  1.1051116703f,  1.0940532343f,
	 1.0829385916f,  1.0718420204f,  1.0606883842f,  1.0495571434f,
	 1.0383880993f,  1.0272496293f,  1.0160814715f,  1.0049460392f,
	 0.9937649659f,  0.9826151929f,  0.9714051258f,  0.9602246825f,
	 0.9489884395f,  0.9377943260f,  0.9265928796f,  0.9154712945f,
	 0.9043106868f,  0.8932043400f,  0.8818521053f,  0.8708298046f,
	 0.8596901081f,  0.8485124885f,  0.8374347030f,  0.8262675846f,
	 0.8151863856f,  0.8041032024f,  0.7929898181f,  0.7819213638f,
	 0.7708626189f,  0.7598478405f,  0.7488402816f,  0.7378519124f,
	 0.7268750405f,  0.7159351783f,  0.7050115342f,  0.6941326055f,
	 0.6832601692f,  0.6724215836f,  0.6615869909f,  0.6507775481f,
	 0.6399890869f,  0.6292346029f,  0.6185164530f,  0.6078470214f,
	 0.5972195405f,  0.5866412762f,  0.5760846623f,  0.5655037792f,
	 0.5549596923f,  0.5443687487f,  0.5340474666f,  0.5237112702f,
	 0.5131676220f,  0.5029672026f,  0.4925918161f,  0.4824099358f,
	 0.4721924196f,  0.4620411273f,  0.4519266790f,  0.4419066050f,
	 0.4318755419f,  0.4219304098f,  0.4119710813f,  0.4021151299f,
	 0.3922658126f,  0.3825072994f,  0.3727544495f,  0.3630855246f,
	 0.3534259170f,  0.3438662338f,  0.3343234494f,  0.3248945791f,
	 0.3154820195f,  0.3061706020f,  0.2968698668f,  0.2876515447f,
	 0.2784363297f,  0.2692982077f,  0.2601619588f,  0.2511740186f,
	 0.2422305215f,  0.2334738679f,  0.2246834377f,  0.2156750628f,
	 0.2071598962f,  0.1984767185f,  0.1898938500f,  0.1814067004f,
	 0.1729007855f,  0.1645454321f,  0.1562017822f,  0.1479358113f,
	 0.1397136271f,  0.1315923511f,  0.1235258532f,  0.1155633999f,
	 0.1076300783f,  0.0998030288f,  0.0920097856f,  0.0843282945f,
	 0.0766873597f,  0.0691492555f,  0.0616418371f,  0.0542297395f,
	 0.0468355725f,  0.0395537898f,  0.0322938011f,  0.0251689168f,
	 0.0180789645f,  0.0111402133f,  0.0042491810f, -0.0025108764f,
	-0.0093228281f, -0.0160240942f, -0.0229218816f, -0.0291537434f,
	-0.0356596108f, -0.0421512920f, -0.0483829023f, -0.0546891309f,
	-0.0607956180f, -0.0668947833f, -0.0729015436f, -0.0788312843f,
	-0.0846659246f, -0.0904454283f, -0.0961440884f, -0.1018001799f,
	-0.1073604731f, -0.1128619755f, -0.1182710743f, -0.1236191798f,
	-0.1288821834f, -0.1340828639f, -0.1391845207f, -0.1442198838f,
	-0.1491346512f, -0.1539912234f, -0.1587317239f, -0.1634326051f,
	-0.1680332327f, -0.1726183100f, -0.1771140890f, -0.1816310071f,
	-0.1859484086f, -0.1902676621f, -0.1941135048f, -0.1982312400f,
	-0.2025752799f, -0.2062425510f, -0.2102567274f, -0.2139529315f,
	-0.2177326536f, -0.2213504338f, -0.2249483716f, -0.2284043787f,
	-0.2318685842f, -0.2351846242f, -0.2384901545f, -0.2416375134f,
	-0.2447669078f, -0.2477656128f, -0.2507463340f, -0.2536057399f,
	-0.2564433319f, -0.2591571117f, -0.2618581518f, -0.2644425265f,
	-0.2670290887f, -0.2695102609f, -0.2719823809f, -0.2743418063f,
	-0.2766682382f, -0.2788599641f, -0.2809996598f, -0.2829509809f,
	-0.2848865889f, -0.2867034021f, -0.2886979015f, -0.2909536921f,
	-0.2921271926f, -0.2939578995f, -0.2955408715f, -0.2970457316f,
	-0.2985327732f, -0.2998832522f, -0.3012206975f, -0.3024750370f,
	-0.3036791840f, -0.3047953307f, -0.3058756301f, -0.3068771119f,
	-0.3078644089f, -0.3087558210f, -0.3096257208f, -0.3103913733f,
	-0.3111336729f, -0.3117751598f, -0.3123958369f, -0.3129048624f,
	-0.3133944699f, -0.3137462882f, -0.3140959719f, -0.3143048413f,
	-0.3145480407f, -0.3146624743f, -0.3148580017f, -0.3149481827f,
	-0.3152146050f, -0.3152392795f, -0.3154945639f, -0.3145152697f,
	-0.3146688330f, -0.3147549076f, -0.3142523940f, -0.3141297440f,
	-0.3136545108f, -0.3133679403f, -0.3128663068f, -0.3124439347f,
	-0.3118458639f, -0.3113288653f, -0.3106447420f, -0.3100296179f,
	-0.3092353383f, -0.3085060392f, -0.3076169401f, -0.3068011429f,
	-0.3058372184f, -0.3049525345f, -0.3039122280f, -0.3029682798f,
	-0.3018593574f, -0.3008800902f, -0.2997260603f, -0.2987161065f,
	-0.2974931002f, -0.2964174081f, -0.2950580040f, -0.2938671020f,
	-0.2922042842f, -0.2908411467f, -0.2887415908f, -0.2879411883f,
	-0.2872351631f, -0.2847616987f, -0.2836812630f, -0.2818896819f,
	-0.2804988036f, -0.2787902978f, -0.2772761254f, -0.2755608442f,
	-0.2740080396f, -0.2722769142f, -0.2706716369f, -0.2689155628f,
	-0.2672727018f, -0.2655133466f, -0.2638390865f, -0.2620527778f,
	-0.2603277216f, -0.2584896769f, -0.2567089643f, -0.2548171418f,
	-0.2529788379f, -0.2510271295f, -0.2491104380f, -0.2470901019f,
	-0.2451131271f, -0.2430630996f, -0.2410984360f, -0.2390892017f,
	-0.2372897729f, -0.2355468158f, -0.2341760732f, -0.2330266272f,
	-0.2285011851f, -0.2276718013f, -0.2256330226f, -0.2236245172f,
	-0.2216212397f, -0.2195723764f, -0.2175598198f, -0.2155244502f,
	-0.2134958020f, -0.2114475228f, -0.2094317510f, -0.2073919547f,
	-0.2054038199f, -0.2033652690f, -0.2013983100f, -0.1993785604f,
	-0.1974621775f, -0.1954904285f, -0.1936526632f, -0.1917311189f,
	-0.1899836720f, -0.1881030278f, -0.1864679413f, -0.1846305866f,
	-0.1831168886f, -0.1812660021f, -0.1798377067f, -0.1778294249f,
	-0.1764397529f, -0.1738005522f, -0.1723006308f, -0.1666149935f,
	-0.1685811576f, -0.1683593655f, -0.1645983125f, -0.1640690747f,
	-0.1614806858f, -0.1606274956f, -0.1583282171f, -0.1573540028f,
	-0.1551676484f, -0.1541795914f, -0.1520476681f, -0.1510623954f,
	-0.1489403301f, -0.1479788256f, -0.1458627832f, -0.1449481895f,
	-0.1428068116f, -0.1419434716f, -0.1397338876f, -0.1389458751f,
	-0.1366453783f, -0.1359864220f, -0.1335694857f, -0.1330985791f,
	-0.1305094750f, -0.1303677621f, -0.1275474455f, -0.1280766855f,
	-0.1248521391f, -0.1270419996f, -0.1230013683f, -0.1333820215f,
	-0.1356269751f, -0.1173893915f, -0.1247875900f, -0.1205369109f,
	-0.1227782673f, -0.1200347844f, -0.1212624937f, -0.1191925052f,
	-0.1200564042f, -0.1183823726f, -0.1190734477f, -0.1176885372f,
	-0.1183154633f, -0.1171681501f, -0.1177737061f, -0.1168115435f,
	-0.1174295604f, -0.1166353753f, -0.1173065305f, -0.1166760717f,
	-0.1174165122f, -0.1169320615f, -0.1177379144f, -0.1173947314f,
	-0.1182902140f, -0.1180910084f, -0.1190858794f, -0.1190024264f,
	-0.1201208291f, -0.1201684429f, -0.1213842597f, -0.1215729938f,
} };

void WidebandSpectrum::execute(buffer_c8_t buffer) {
	// 2048 complex8_t samples per buffer.
	// 102.4us per buffer. 20480 instruction cycles per buffer.

	static int phase = 0;

	if( phase == 0 ) {
		std::fill(spectrum.begin(), spectrum.end(), 0);
	}

	if( (phase & 7) == 0 ) {
		const size_t window_offset = (phase >> 3) * 256;
		const auto window_p = &window_wideband_bin_lpf[window_offset];
		for(size_t i=0; i<channel_spectrum.size(); i++) {
			spectrum[i] += std::complex<float> { buffer.p[i].real() * window_p[i], buffer.p[i].imag() * window_p[i] };
		}
	}

	if( phase == 23 ) {
		if( channel_spectrum_request_update == false ) {
			fft_swap(spectrum, channel_spectrum);
			channel_spectrum_sampling_rate = buffer.sampling_rate;
			channel_filter_pass_frequency = 0;
			channel_filter_stop_frequency = 0;
			channel_spectrum_request_update = true;
			events_flag(EVT_MASK_SPECTRUM);
			phase = 0;
		}
	} else {
		phase++;
	}

	i2s::i2s0::tx_mute();
}
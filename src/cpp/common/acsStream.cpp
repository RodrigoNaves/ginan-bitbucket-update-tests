
#include "navigation.hpp"
#include "acsStream.hpp"
#include "acsConfig.hpp"
#include "enums.h"
#include <map>
#include <boost/utility/binary.hpp>


static int sEpochNum =  1; //todo mark: remove
static int rtcmdeblvl = 3;

ObsList ObsStream::getObs()
{
	if (obsListList.size() > 0)
	{
		ObsList& obsList = obsListList.front();

		std::sort(obsList.begin(), obsList.end(), [](Obs& a, Obs& b)
			{
				if (a.Sat.sys < b.Sat.sys)		return true;
				if (a.Sat.sys > b.Sat.sys)		return false;
				if (a.Sat.prn < b.Sat.prn)		return true;
				else							return false;
			});

		for (auto& obs					: obsList)
		for (auto& [ftype, sigsList]	: obs.SigsLists)
		{
			sigsList.sort([](RawSig& a, RawSig& b)
				{
					auto iterA = std::find(acsConfig.code_priorities.begin(), acsConfig.code_priorities.end(), a.code);
					auto iterB = std::find(acsConfig.code_priorities.begin(), acsConfig.code_priorities.end(), b.code);

					if (a.L == 0)		return false;
					if (b.L == 0)		return true;
					if (a.P == 0)		return false;
					if (b.P == 0)		return true;
					if (iterA < iterB)	return true;
					else				return false;
				});

			RawSig firstOfType = sigsList.front();

			//use first of type as representative if its in the priority list
			auto iter = std::find(acsConfig.code_priorities.begin(), acsConfig.code_priorities.end(), firstOfType.code);
			if (iter != acsConfig.code_priorities.end())
			{
				obs.Sigs[ftype] = Sig(firstOfType);
			}
		}

		return obsList;
	}
	else
	{
		return ObsList();
	}
}


// From the RTCM spec...
//  - table 3.5-91 (GPS)
//  - table 3.5-96 (GLONASS)
//  - table 3.5-99 (GALILEO)
//  - table 3.5-105 (QZSS)
//  - table 3.5-108 (BEIDOU)



struct SignalInfo
{
    uint8_t		signal_id;
    E_FType		ftype;
    E_ObsCode	rinex_observation_code;
};


const int DefGLOChnl [24] =
{ 1, -4, 5, 6, 1, -4, 5, 6, -2, -7, 0, -1, -2, -7, 0, -1, 4, -3, 3, 2, 4, -3, 3, 2 };

// sys -> rtcm signal enum -> siginfo (sig enum,
map<E_Sys, map<uint8_t, SignalInfo>> signal_id_mapping =
{
    {	E_Sys::GPS,
		{
			{2,		{2,		L1,		E_ObsCode::L1C}},
			{3,		{3,		L1,		E_ObsCode::L1P}},
			{4,		{4,		L1,		E_ObsCode::L1W}},

			{8,		{8,		L2,		E_ObsCode::L2C}},
			{9,		{9,		L2,		E_ObsCode::L2P}},
			{10,	{10,	L2,		E_ObsCode::L2W}},
			{15,	{15,	L2,		E_ObsCode::L2S}},
			{16,	{16,	L2,		E_ObsCode::L2L}},
			{17,	{17,	L2,		E_ObsCode::L2X}},

			{22,	{22,	L5,		E_ObsCode::L5I}},
			{23,	{23,	L5,		E_ObsCode::L5Q}},
			{24,	{24,	L5,		E_ObsCode::L5X}},

			{30,	{2,		L1,		E_ObsCode::L1S}},
			{31,	{2,		L1,		E_ObsCode::L1L}},
			{32,	{2,		L1,		E_ObsCode::L1X}}
		}
	},

    {	E_Sys::GLO,
		{
			{2,		{2,		G1,		E_ObsCode::L1C}},
			{3,		{3,		G1,		E_ObsCode::L1P}},

			{8,		{8,		G2,		E_ObsCode::L2C}},
			{9,		{9,		G2,		E_ObsCode::L2P}}
		}
	},

    {	E_Sys::GAL,
		{
			{2,		{2,		E1,		E_ObsCode::L1C}},
			{3,		{3,		E1,		E_ObsCode::L1A}},
			{4,		{4,		E1,		E_ObsCode::L1B}},
			{5,		{5,		E1,		E_ObsCode::L1X}},
			{6,		{6,		E1,		E_ObsCode::L1Z}},

			{8,		{8,		E6,		E_ObsCode::L6C}},
			{9,		{9,		E6,		E_ObsCode::L6A}},
			{10,	{10,	E6,		E_ObsCode::L6B}},
			{11,	{11,	E6,		E_ObsCode::L6X}},
			{12,	{12,	E6,		E_ObsCode::L6Z}},

			{14,	{14,	E5B,	E_ObsCode::L7I}},
			{15,	{15,	E5B,	E_ObsCode::L7Q}},
			{16,	{16,	E5B,	E_ObsCode::L7X}},

			{18,	{18,	E5AB,	E_ObsCode::L8I}},
			{19,	{19,	E5AB,	E_ObsCode::L8Q}},
			{20,	{20,	E5AB,	E_ObsCode::L8X}},

			{22,	{22,	E5A,	E_ObsCode::L5I}},
			{23,	{23,	E5A,	E_ObsCode::L5Q}},
			{24,	{24,	E5A,	E_ObsCode::L5X}}
		}
	},

    {	E_Sys::QZS,
		{
			{2,		{2,		L1,		E_ObsCode::L1C}},

			{9,		{9,		LEX,	E_ObsCode::L6S}},
			{10,	{10,	LEX,	E_ObsCode::L6L}},
			{11,	{11,	LEX,	E_ObsCode::L6X}},

			{15,	{15,	L2,		E_ObsCode::L2S}},
			{16,	{16,	L2,		E_ObsCode::L2L}},
			{17,	{17,	L2,		E_ObsCode::L2X}},

			{22,	{22,	L5,		E_ObsCode::L5I}},
			{23,	{23,	L5,		E_ObsCode::L5Q}},
			{24,	{24,	L5,		E_ObsCode::L5X}},

			{30,	{30,	L1,		E_ObsCode::L1S}},
			{31,	{31,	L1,		E_ObsCode::L1L}},
			{32,	{32,	L1,		E_ObsCode::L1X}}
		}
	},

    {	E_Sys::CMP,
		{
			{2,		{2,		B1,		E_ObsCode::L2I}},
			{3,		{3,		B1,		E_ObsCode::L2Q}},
			{4,		{4,		B1,		E_ObsCode::L2X}},

			{8,		{8,		B3,		E_ObsCode::L6I}},
			{9,		{9,		B3,		E_ObsCode::L6Q}},
			{10,	{10,	B3,		E_ObsCode::L6X}},

			{14,	{14,	B2,		E_ObsCode::L7I}},
			{15,	{15,	B2,		E_ObsCode::L7Q}},
			{16,	{16,	B2,		E_ObsCode::L7X}}
		}
	}
};

// sys -> rtcm signal enum -> siginfo (sig enum,
map<E_Sys, map<E_ObsCode, E_FType>> codeTypeMap =
{
    {	E_Sys::GPS,
		{
			{E_ObsCode::L1C,	L1	},
			{E_ObsCode::L1P,	L1	},
			{E_ObsCode::L1W,	L1	},

			{E_ObsCode::L2C,	L2	},
			{E_ObsCode::L2P,	L2	},
			{E_ObsCode::L2W,	L2	},
			{E_ObsCode::L2S,	L2	},
			{E_ObsCode::L2L,	L2	},
			{E_ObsCode::L2X,	L2	},

			{E_ObsCode::L5I,	L5	},
			{E_ObsCode::L5Q,	L5	},
			{E_ObsCode::L5X,	L5	},

			{E_ObsCode::L1S,	L1	},
			{E_ObsCode::L1L,	L1	},
			{E_ObsCode::L1X,	L1	}
		}
	},

    {	E_Sys::GLO,
		{
			{E_ObsCode::L1C,	G1	},
			{E_ObsCode::L1P,	G1	},
            
			{E_ObsCode::L2C,	G2	},
			{E_ObsCode::L2P,	G2	}
		}
	},

    {	E_Sys::GAL,
		{
			{E_ObsCode::L1C,	E1	},
			{E_ObsCode::L1A,	E1	},
			{E_ObsCode::L1B,	E1	},
			{E_ObsCode::L1X,	E1	},
			{E_ObsCode::L1Z,	E1	},

			{E_ObsCode::L6C,	E6	},
			{E_ObsCode::L6A,	E6	},
			{E_ObsCode::L6B,	E6	},
			{E_ObsCode::L6X,	E6	},
			{E_ObsCode::L6Z,	E6	},

			{E_ObsCode::L7I,	E5B	},
			{E_ObsCode::L7Q,	E5B	},
			{E_ObsCode::L7X,	E5B	},

			{E_ObsCode::L8I,	E5AB},
			{E_ObsCode::L8Q,	E5AB},
			{E_ObsCode::L8X,	E5AB},

			{E_ObsCode::L5I,	E5A	},
			{E_ObsCode::L5Q,	E5A	},
			{E_ObsCode::L5X,	E5A	}
		}
	},

    {	E_Sys::QZS,
		{
			{E_ObsCode::L1C,	L1	},

			{E_ObsCode::L6S,	LEX	},
			{E_ObsCode::L6L,	LEX	},
			{E_ObsCode::L6X,	LEX	},

			{E_ObsCode::L2S,	L2	},
			{E_ObsCode::L2L,	L2	},
			{E_ObsCode::L2X,	L2	},

			{E_ObsCode::L5I,	L5	},
			{E_ObsCode::L5Q,	L5	},
			{E_ObsCode::L5X,	L5	},

			{E_ObsCode::L1S,	L1	},
			{E_ObsCode::L1L,	L1	},
			{E_ObsCode::L1X,	L1	}
		}
	},

    {	E_Sys::CMP,
		{
			{E_ObsCode::L2I,	B1	},
			{E_ObsCode::L2Q,	B1	},
			{E_ObsCode::L2X,	B1	},

			{E_ObsCode::L6I,	B3	},
			{E_ObsCode::L6Q,	B3	},
			{E_ObsCode::L6X,	B3	},

			{E_ObsCode::L7I,	B2	},
			{E_ObsCode::L7Q,	B2	},
			{E_ObsCode::L7X,	B2	}
		}
	}
};

// From the RTCM spec table 3.5-73

map<E_Sys, map<E_FType, double>> signal_phase_alignment =
{
    {	E_Sys::GPS,
		{
			{L1,		1.57542E9},
			{L2,		1.22760E9},
			{L5,		1.17645E9}
		}
	},

    // TODO

    //    {SatelliteSystem::GLONASS, {
    //        {FrequencyBand::G1, 1.57542E9},
    //        {FrequencyBand::G2, 1.22760E9},
    //    }},

    {	E_Sys::GAL,
		{
			{E1,		1.57542E9},
			{E5A,		1.17645E9},
			{E5B,		1.20714E9},
			{E5AB,		1.1191795E9},
			{E6,		1.27875E9},
		}
	},

    {	E_Sys::QZS,
		{
			{L1,		1.57542E9},
			{L2,		1.22760E9},
			{L5,		1.17645E9}
		}
	},

    {	E_Sys::CMP,
		{
			{B1,		1.561098E9},
			{B2,		1.207140E9},
			{B3,		1.26852E9}
		}
	}
};










struct RtcmDecoder
{
	static void setTime(GTime& time, double tow)
	{
		auto now = utc2gpst(timeget());

		int week;
		double tow_p = time2gpst(now, &week);		//todo aaron, cant use now all the time

		int sPerWeek = 60*60*24*7;
		if      (tow < tow_p - sPerWeek/2)				tow += sPerWeek;
		else if (tow > tow_p + sPerWeek/2)				tow -= sPerWeek;

		time = gpst2time(week, tow);
	}

    struct Decoder
    {
		uint8_t* 		data			= nullptr;
		unsigned int 	message_length	= 0;
    };



	struct SSRDecoder : Decoder
	{


        ~SSRDecoder()
        {

        }
        
		constexpr static int updateInterval[16] =
		{
			1, 2, 5, 10, 15, 30, 60, 120, 240, 300, 600, 900, 1800, 3600, 7200, 10800
		};

		void decode()
		{
			int i = 0;
			int message_number		= getbituInc(data, i, 12);
			int	epochTime1s			= getbituInc(data, i, 20);
			int	ssrUpdateIntIndex	= getbituInc(data, i, 4);
			int	multipleMessage		= getbituInc(data, i, 1);

			int ssrUpdateInterval	= updateInterval[ssrUpdateIntIndex];
			double epochTime = epochTime1s + ssrUpdateInterval / 2.0;

			E_Sys::_enumerated sys = E_Sys::NONE;
			if 	( message_number == +RtcmMessageType::GPS_SSR_ORB_CORR
				||message_number == +RtcmMessageType::GPS_SSR_CLK_CORR
				||message_number == +RtcmMessageType::GPS_SSR_COMB_CORR
				||message_number == +RtcmMessageType::GPS_SSR_CODE_BIAS
				||message_number == +RtcmMessageType::GPS_SSR_PHASE_BIAS						
				||message_number == +RtcmMessageType::GPS_SSR_URA)
			{
				sys = E_Sys::GPS;
			}
			else if ( message_number == +RtcmMessageType::GAL_SSR_ORB_CORR
					||message_number == +RtcmMessageType::GAL_SSR_CLK_CORR
					||message_number == +RtcmMessageType::GAL_SSR_CODE_BIAS
					||message_number == +RtcmMessageType::GAL_SSR_PHASE_BIAS
					||message_number == +RtcmMessageType::GAL_SSR_COMB_CORR)
			{
				sys = E_Sys::GAL;
			}
			else
			{
				std::cout << "Error: unrecognised message in SSRDecoder::decode()" << std::endl;
			}

			int np = 0;
			int ni = 0;
			int nj = 0;
			int offp = 0;
			switch (sys) 
			{
				case E_Sys::GPS: np=6; ni= 8; nj= 0; offp=  0; break;
				case E_Sys::GLO: np=5; ni= 8; nj= 0; offp=  0; break;
				case E_Sys::GAL: np=6; ni=10; nj= 0; offp=  0; break;
				case E_Sys::QZS: np=4; ni= 8; nj= 0; offp=192; break;
				case E_Sys::CMP: np=6; ni=10; nj=24; offp=  1; break;
				case E_Sys::SBS: np=6; ni= 9; nj=24; offp=120; break;
				default: std::cout << "Error: unrecognised system in SSRDecoder::decode()" << std::endl;
			}

			unsigned int referenceDatum = 0;
			if 	( message_number == +RtcmMessageType::GPS_SSR_ORB_CORR
				||message_number == +RtcmMessageType::GPS_SSR_COMB_CORR
				||message_number == +RtcmMessageType::GAL_SSR_ORB_CORR
				||message_number == +RtcmMessageType::GAL_SSR_COMB_CORR)
			{
				referenceDatum	= getbituInc(data, i, 1);
			}
			unsigned int	iod					= getbituInc(data, i, 4);
			unsigned int	provider			= getbituInc(data, i, 16);
			unsigned int	solution			= getbituInc(data, i, 4);
            
            unsigned int dispBiasConistInd;
            unsigned int MWConistInd;
            if  ( message_number == +RtcmMessageType::GPS_SSR_PHASE_BIAS 
                ||message_number == +RtcmMessageType::GAL_SSR_PHASE_BIAS )
            {
                dispBiasConistInd  = getbituInc(data, i, 1);
                MWConistInd        = getbituInc(data, i, 1);
            }
            
			unsigned int	numSats				= getbituInc(data, i, 6);

            // SRR variables for encoding and decoding.
            SSRDebug ssrDebug;
            ssrDebug.epochTime1s = epochTime1s;
            ssrDebug.ssrUpdateIntIndex = ssrUpdateIntIndex;
            ssrDebug.multipleMessage = multipleMessage;
            ssrDebug.referenceDatum = referenceDatum;
            ssrDebug.provider = provider;
            ssrDebug.solution = solution;
            
			for (int sat = 0; sat < numSats; sat++)
			{
				unsigned int	satId			= getbituInc(data, i, np)+offp;

				SatSys Sat(sys, satId);

				if 	( message_number == +RtcmMessageType::GPS_SSR_ORB_CORR
					||message_number == +RtcmMessageType::GPS_SSR_COMB_CORR
					||message_number == +RtcmMessageType::GAL_SSR_ORB_CORR
					||message_number == +RtcmMessageType::GAL_SSR_COMB_CORR)
				{
					SSREph ssrEph;
                    ssrEph.ssrDebug = ssrDebug;

					setTime(ssrEph.t0, epochTime);

					ssrEph.iod 			= iod;
					ssrEph.iode			= getbituInc(data, i, ni);
					// ??ssrEph.iodcrc		= getbituInc(data, i, nj);
					ssrEph.deph[0]		= getbitsInc(data, i, 22) * 0.1e-3; // Position, radial, along track, cross track.
					ssrEph.deph[1]		= getbitsInc(data, i, 20) * 0.4e-3;
					ssrEph.deph[2]		= getbitsInc(data, i, 20) * 0.4e-3;
					ssrEph.ddeph[0]		= getbitsInc(data, i, 21) * 0.001e-3; // Velocity
					ssrEph.ddeph[1]		= getbitsInc(data, i, 19) * 0.004e-3;
					ssrEph.ddeph[2]		= getbitsInc(data, i, 19) * 0.004e-3;
                   
                    tracepdeex(rtcmdeblvl+1,std::cout, "\n#RTCM_DEC SSRORB %s %s %4d %10.3f %10.3f %10.3f ", Sat.id(),ssrEph.t0.to_string(2), ssrEph.iode,ssrEph.deph[0],ssrEph.deph[1],ssrEph.deph[2]);
					if	( ssrEph.iod		!= nav.satNavMap[Sat].ssr.ssrEph.iod
						||ssrEph.t0.time	!= nav.satNavMap[Sat].ssr.ssrEph.t0.time)
					{
						nav.satNavMap[Sat].ssr.ssrEph = ssrEph;
					}
				}

				if 	( message_number == +RtcmMessageType::GPS_SSR_CLK_CORR
					||message_number == +RtcmMessageType::GPS_SSR_COMB_CORR
					||message_number == +RtcmMessageType::GAL_SSR_CLK_CORR
					||message_number == +RtcmMessageType::GAL_SSR_COMB_CORR)
				{
					SSRClk ssrClk;
                    ssrClk.ssrDebug = ssrDebug;

					setTime(ssrClk.t0, epochTime);

					ssrClk.iod 			= iod;
                    
                    // C = C_0 + C_1(t-t_0)+C_2(t-t_0)^2 where C is a correction in meters.
                    // C gets converted into a time correction for futher calculations.
					ssrClk.dclk[0]		= getbitsInc(data, i, 22) * 0.1e-3;
					ssrClk.dclk[1]		= getbitsInc(data, i, 21) * 0.001e-3;
					ssrClk.dclk[2]		= getbitsInc(data, i, 27) * 0.00002e-3;
                  
                    tracepdeex(rtcmdeblvl+1,std::cout, "\n#RTCM_DEC SSRCLK %s %s      %10.3f %10.3f %10.3f", Sat.id(),ssrClk.t0.to_string(2), ssrClk.dclk[0],ssrClk.dclk[1],ssrClk.dclk[2]);
					if	( ssrClk.iod		!= nav.satNavMap[Sat].ssr.ssrClk.iod
						||ssrClk.t0.time	!= nav.satNavMap[Sat].ssr.ssrClk.t0.time)
					{
						nav.satNavMap[Sat].ssr.ssrClk = ssrClk;
					}
				}

				if 	( message_number == +RtcmMessageType::GPS_SSR_URA)
				{
					SSRUra ssrUra;

					setTime(ssrUra.t0, epochTime);

					ssrUra.iod 			= iod;
					ssrUra.ura			= getbituInc(data, i, 6);

					if	( ssrUra.iod		!= nav.satNavMap[Sat].ssr.ssrUra.iod
						||ssrUra.t0.time	!= nav.satNavMap[Sat].ssr.ssrUra.t0.time)
					{
                        // This is the total User Range Accuracy calculated from all the SSR.
                        // TODO: Check implementation, RTCM manual DF389.                     
						nav.satNavMap[Sat].ssr.ssrUra = ssrUra;
					}
				}
				
				if  ( message_number == +RtcmMessageType::GPS_SSR_CODE_BIAS
                    ||message_number == +RtcmMessageType::GAL_SSR_CODE_BIAS)
                {
                    SSRBias ssrBias = nav.satNavMap[Sat].ssr.ssrBias;
                    ssrBias.ssrDebug = ssrDebug;
                    
					setTime(ssrBias.t0_code, epochTime);
					ssrBias.iod_code 			= iod;
                    
                    unsigned int nbias       = getbituInc(data, i, 5);
                    
                    for (int k = 0; k < nbias && i + 19 <= message_length * 8; k++) 
                    {
                        int rtcm_code               = getbituInc(data, i, 5);
                        double bias                 = getbitsInc(data, i, 14) * 0.01;
                        
                        E_ObsCode mode;
                        if ( sys == E_Sys::GPS )
                        {
                            mode = mCodes_gps.right.at(rtcm_code);
                        }
                        else if ( sys == E_Sys::GAL )
                        {
                            mode = mCodes_gal.right.at(rtcm_code);
                        }
                        else
                            std::cout << "Error: unrecognised system in SSRDecoder::decode()" << std::endl;
                        ssrBias.cbias[mode] = bias;
                                        
                    }
                    nav.satNavMap[Sat].ssr.ssrBias = ssrBias;

                }
                
				if  ( message_number == +RtcmMessageType::GPS_SSR_PHASE_BIAS
                    ||message_number == +RtcmMessageType::GAL_SSR_PHASE_BIAS )
                {
                    SSRBias ssrBias = nav.satNavMap[Sat].ssr.ssrBias;
                    SSRPhase ssrPhase;
                    ssrBias.ssrDebug = ssrDebug;
                    
					setTime(ssrBias.t0_phas, epochTime);
					ssrBias.iod_phas 			= iod;
                    
                    ssrPhase.dispBiasConistInd = dispBiasConistInd;
                    ssrPhase.MWConistInd = dispBiasConistInd;
                    ssrPhase.nbias            = getbituInc(data, i, 5);
                    ssrPhase.yawAngle         = getbituInc(data, i, 9)/256;
                    ssrPhase.yawRate          = getbitsInc(data, i, 8)/8192;
                    ssrBias.ssrPhase = ssrPhase;
                    
                    for (int k = 0; k < ssrPhase.nbias && i + 32 <= message_length * 8; k++) 
                    {
                        SSRPhaseExtra ssrPExtra;
                        unsigned int rtcm_code     = getbituInc(data, i, 5);
                        ssrPExtra.signalIntInd     = getbituInc(data, i, 1);
                        ssrPExtra.signalWidIntInd  = getbituInc(data, i, 2);
                        ssrPExtra.signalDisconCnt  = getbituInc(data, i, 4);
                        double PhaseBias        = getbitsInc(data, i, 20)*0.0001;
                        
                        E_ObsCode mode;
                        if ( sys == E_Sys::GPS )
                        {
                            mode = mCodes_gps.right.at(rtcm_code);
                        }
                        else if ( sys == E_Sys::GAL )
                        {
                            mode = mCodes_gal.right.at(rtcm_code);
                        }
                        else
                            std::cout << "Error: unrecognised system in SSRDecoder::decode()" << std::endl; 
                        
                        ssrBias.pbias[mode] = PhaseBias; // offset meters due to satellite rotation.
                        ssrBias.pExtra[mode] = ssrPExtra;
                    }
                    nav.satNavMap[Sat].ssr.ssrBias = ssrBias;
                    
                }
			}
		}
	};

	struct EphemerisDecoder : Decoder
	{
		void decode()
		{
			Eph eph = {};
			int i = 0;
			int message_number		= getbituInc(data, i, 12);

			E_Sys::_enumerated sys = E_Sys::NONE;
			if 	( message_number == RtcmMessageType::GPS_EPHEMERIS )
			{
				sys = E_Sys::GPS;
			}
			else if ( message_number == RtcmMessageType::GAL_FNAV_EPHEMERIS
					||message_number == RtcmMessageType::GAL_INAV_EPHEMERIS)
			{
				sys = E_Sys::GAL;
			}
			else
			{
				std::cout << "Error: unrecognised message in EphemerisDecoder::decode()" << std::endl;
			}

			if (sys == E_Sys::GPS)
			{
				if (i + 476 > message_length * 8)
				{
					tracepde(2,std::cout, "rtcm3 1019 length error: len=%d\n", message_length);
					return;
				}

				int prn			= getbituInc(data, i,  6);
				eph.week		= adjgpsweek(getbituInc(data, i, 10));
				eph.sva   		= getbituInc(data, i,  4);
				eph.code  		= getbituInc(data, i,  2);
				eph.idot  		= getbitsInc(data, i, 14)*P2_43*SC2RAD;
				eph.iode  		= getbituInc(data, i,  8);
				double toc     	= getbituInc(data, i, 16)*16.0;
				eph.f2    		= getbitsInc(data, i,  8)*P2_55;
				eph.f1    		= getbitsInc(data, i, 16)*P2_43;
				eph.f0    		= getbitsInc(data, i, 22)*P2_31;
				eph.iodc  		= getbituInc(data, i, 10);
				eph.crs   		= getbitsInc(data, i, 16)*P2_5;
				eph.deln  		= getbitsInc(data, i, 16)*P2_43*SC2RAD;
				eph.M0    		= getbitsInc(data, i, 32)*P2_31*SC2RAD;
				eph.cuc   		= getbitsInc(data, i, 16)*P2_29;
				eph.e     		= getbituInc(data, i, 32)*P2_33;
				eph.cus   		= getbitsInc(data, i, 16)*P2_29;
				double sqrtA    = getbituInc(data, i, 32)*P2_19;
				eph.toes  		= getbituInc(data, i, 16)*16.0;
				eph.cic   		= getbitsInc(data, i, 16)*P2_29;
				eph.OMG0  		= getbitsInc(data, i, 32)*P2_31*SC2RAD;
				eph.cis   		= getbitsInc(data, i, 16)*P2_29;
				eph.i0    		= getbitsInc(data, i, 32)*P2_31*SC2RAD;
				eph.crc   		= getbitsInc(data, i, 16)*P2_5;
				eph.omg   		= getbitsInc(data, i, 32)*P2_31*SC2RAD;
				eph.OMGd  		= getbitsInc(data, i, 24)*P2_43*SC2RAD;
				eph.tgd[0]		= getbitsInc(data, i,  8)*P2_31;
				eph.svh   		= getbituInc(data, i,  6);
				eph.flag  		= getbituInc(data, i,  1);
				eph.fit   		= getbituInc(data, i,  1)?0.0:4.0; /* 0:4hr,1:>4hr */

				if (prn >= 40)
				{
					sys = E_Sys::SBS;
					prn += 80;
				}
				
				if (1)	//todo aaron, janky?
				{
					int week;
					double tow	= time2gpst(utc2gpst(timeget()), &week);
					eph.ttr		= gpst2time(week, floor(tow));
				}
				eph.Sat		= SatSys(sys, prn);
				eph.toe		= gpst2time(eph.week,eph.toes);
				eph.toc		= gpst2time(eph.week,toc);
				eph.A		= SQR(sqrtA);
			}
			else if (sys == E_Sys::GAL)
			{
                if ( message_number == RtcmMessageType::GAL_FNAV_EPHEMERIS )
                {
                    if (i + 496-12 > message_length * 8)
                    {
                        tracepde(2,std::cout, "rtcm3 1045 length error: len=%d\n", message_length);
                        return;
                    }
                }
                else if (message_number == RtcmMessageType::GAL_INAV_EPHEMERIS)
                {
                    if (i + 504-12 > message_length * 8)
                    {
                        tracepde(2,std::cout, "rtcm3 1046 length error: len=%d\n", message_length);
                        return;
                    }                    
                }
                else
                {
                    std::cout << "Error: unrecognised message for GAL in EphemerisDecoder::decode()" << std::endl;                    
                }
                
                int prn			= getbituInc(data, i, 6);
                eph.week		= adjgpsweek(getbituInc(data, i, 12));
                eph.iode  		= getbituInc(data, i, 10); // Documented as IODnav
                eph.sva   		= getbituInc(data, i,  8); // Documented SISA
                
                eph.idot  		= getbitsInc(data, i, 14)*P2_43*SC2RAD;
                double toc     	= getbituInc(data, i, 14)*60.0;
                eph.f2    		= getbitsInc(data, i,  6)*P2_59;
                eph.f1    		= getbitsInc(data, i, 21)*P2_46;
                eph.f0    		= getbitsInc(data, i, 31)*P2_34;
                eph.crs   		= getbitsInc(data, i, 16)*P2_5;
                eph.deln  		= getbitsInc(data, i, 16)*P2_43*SC2RAD;
                eph.M0    		= getbitsInc(data, i, 32)*P2_31*SC2RAD;
                eph.cuc   		= getbitsInc(data, i, 16)*P2_29;
                eph.e     		= getbituInc(data, i, 32)*P2_33;
                eph.cus   		= getbitsInc(data, i, 16)*P2_29;
                double sqrtA    = getbituInc(data, i, 32)*P2_19;
                eph.toes  		= getbituInc(data, i, 14)*60.0;
                eph.cic   		= getbitsInc(data, i, 16)*P2_29;
                eph.OMG0  		= getbitsInc(data, i, 32)*P2_31*SC2RAD;
                eph.cis   		= getbitsInc(data, i, 16)*P2_29;
                eph.i0    		= getbitsInc(data, i, 32)*P2_31*SC2RAD;
                eph.crc   		= getbitsInc(data, i, 16)*P2_5;
                eph.omg   		= getbitsInc(data, i, 32)*P2_31*SC2RAD;
                eph.OMGd  		= getbitsInc(data, i, 24)*P2_43*SC2RAD;
                eph.tgd[0]		= getbitsInc(data, i, 10)*P2_32;

                int e5a_hs	= 0;
                int e5a_dvs	= 0;
                int rsv		= 0;
                int e5b_hs	= 0;
                int e5b_dvs	= 0;
                int e1_hs	= 0;
                int e1_dvs	= 0;
                if (message_number == RtcmMessageType::GAL_FNAV_EPHEMERIS )
                {
                    e5a_hs		= getbituInc(data, i,  2);		// OSHS
                    e5a_dvs		= getbituInc(data, i,  1);		// OSDVS
                    rsv			= getbituInc(data, i,  7);
                }
                else if (message_number == RtcmMessageType::GAL_INAV_EPHEMERIS)
                {
                    eph.tgd[1]	= getbitsInc(data, i,10)*P2_32;	// E5b/E1
                    e5b_hs		= getbituInc(data, i, 2);		// E5b OSHS
                    e5b_dvs		= getbituInc(data, i, 1);		// E5b OSDVS
                    e1_hs		= getbituInc(data, i, 2);		// E1 OSHS
                    e1_dvs		= getbituInc(data, i, 1);		// E1 OSDVS
                }

				if (1)	//todo aaron, janky?
				{
					int week;
					double tow	= time2gpst(utc2gpst(timeget()), &week);
					eph.ttr		= gpst2time(week, floor(tow));
				}
                eph.Sat		= SatSys(sys, prn);
				eph.toe		= gpst2time(eph.week,eph.toes);
				eph.toc		= gpst2time(eph.week,toc);                
                
                eph.A		= SQR(sqrtA);
                if (message_number == RtcmMessageType::GAL_FNAV_EPHEMERIS )
                {
                    eph.svh=(e5a_hs<<4)+(e5a_dvs<<3);
                    eph.code=(1<<1)+(1<<8); // data source = F/NAV+E5a
                    eph.iodc=eph.iode;
                }
                else if (message_number == RtcmMessageType::GAL_INAV_EPHEMERIS)
                {
                    eph.svh=(e5b_hs<<7)+(e5b_dvs<<6)+(e1_hs<<1)+(e1_dvs<<0);
                    eph.code=(1<<0)+(1<<2)+(1<<9); // data source = I/NAV+E1+E5b
                    eph.iodc=eph.iode;
                }           
			}
			else
			{
				std::cout << "Error: unrecognised sys in EphemerisDecoder::decode()" << std::endl;
			}

			//check for iode, add if not found.
			for (auto& eph_ : nav.ephMap[eph.Sat])
			{
				if (eph_.iode == eph.iode && fabs(timediff(eph.toe,eph_.toe))<6*60*60) //current iode is guaranteed to be different from other iode's transmitted in the last 6 hours, but then it begins to repeat
				{
					return;
				}
			}		
			tracepdeex(rtcmdeblvl+1,std::cout, "\n#RTCM_DEC BRCEPH %s %s %4d %16.9e %13.6e %10.3e ", eph.Sat.id(),eph.toc.to_string(2), eph.iode, eph.f0,eph.f1,eph.f2);
			//std::cout << "Adding ephemeris for " << eph.Sat.id() << std::endl;
			nav.ephMap[eph.Sat].push_back(eph);
		}
	};

	struct MSMDecoder : Decoder
	{
        E_FType code_to_ftype(E_Sys sys, E_ObsCode code)
		{
            if	( codeTypeMap		.count(sys)		> 0
				&&codeTypeMap[sys]	.count(code)	> 0)
			{
                return codeTypeMap.at(sys).at(code);
            }

            return FTYPE_NONE;
        }

        boost::optional<SignalInfo> get_signal_info(E_Sys sys, uint8_t signal)
		{
            if	( signal_id_mapping		.count(sys)		> 0
				&&signal_id_mapping[sys].count(signal)	> 0)
			{
                return signal_id_mapping.at(sys).at(signal);
            }

            return boost::optional<SignalInfo>();
        }

        E_ObsCode signal_to_code(E_Sys sys, uint8_t signal)
		{
            boost::optional<SignalInfo> info = get_signal_info(sys, signal);

            if (info)
			{
                return info->rinex_observation_code;
            }

            return E_ObsCode::NONE;
        }


        ObsList decode(map<SatSys,map<E_ObsCode,int>> lock_time)
		{
			ObsList obsList;
			int i = 0;
			
			int message_number				= getbituInc(data, i,	12);
			int reference_station_id		= getbituInc(data, i,	12);
			int epoch_time_					= getbituInc(data, i,	30);
			int multiple_message			= getbituInc(data, i,	1);
			int issue_of_data_station		= getbituInc(data, i,	3);
			int reserved					= getbituInc(data, i,	7);
			int clock_steering_indicator	= getbituInc(data, i,	2);
			int external_clock_indicator	= getbituInc(data, i,	2);
			int smoothing_indicator			= getbituInc(data, i,	1);
			int smoothing_interval			= getbituInc(data, i,	3);

			int msmtyp = message_number%10;
			bool extrainfo=false;
			if(msmtyp==5 || msmtyp==7) extrainfo=true;
			int nbcd=15, nbph=22, nblk=4, nbcn=6;
			double sccd=P2_24, scph=P2_29, scsn=1.0; 
			if(msmtyp==6 || msmtyp==7){
				nbcd=20; sccd=P2_29;
				nbph=24; scph=P2_31;
				nblk=10;
				nbcn=10; scsn=0.0625;
			}

			int sysind = message_number/10;
			E_Sys rtcmsys=E_Sys::NONE;
			double tow = epoch_time_ * 0.001;
			switch(sysind)
			{
				case 107:	rtcmsys = E_Sys::GPS; break;
				case 108:	rtcmsys = E_Sys::GLO; break;
				case 109:	rtcmsys = E_Sys::GAL; break;
				case 111:	rtcmsys = E_Sys::QZS; break;
				case 112:	rtcmsys = E_Sys::CMP; tow+=14.0; break;
			}
			
			GTime tobs;
			if(rtcmsys == +E_Sys::GLO)
			{
				int dowi=(epoch_time_  >> 27);
				int todi=(epoch_time_ & 0x7FFFFFF);
				tow = 86400.0*dowi + 0.001*todi - 10800.0;
				GTime tglo;
				setTime(tglo, tow);
				tobs=utc2gpst(tglo);	
			} 
			else setTime(tobs, tow);

			

			//create observations for satelllites according to the mask
			for (int sat = 0; sat < 64; sat++)
			{
				bool mask 					= getbituInc(data, i,	1);
				if (mask)
				{
					Obs obs;
					obs.Sat.sys = rtcmsys;
					obs.Sat.prn = sat + 1;
					obs.time=tobs;
					obsList.push_back(obs);
				}
			}

			//create a temporary list of signals
			list<E_ObsCode> signalMaskList;
			for (int sig = 0; sig < 32; sig++)
			{
				bool mask 					= getbituInc(data, i,	1);
				if (mask)
				{
					int code = signal_to_code(rtcmsys, sig + 1);
					signalMaskList.push_back(E_ObsCode::_from_integral(code));
				}
			}

			//create a temporary list of signal pointers for simpler iteration later
			map<int,RawSig*> signalPointermap;
			map<int,SatSys>  cellSatellitemap;
			int indx=0;
			//create signals for observations according to existing observations, the list of signals, and the cell mask
			for (auto& obs		: obsList)
			for (auto& sigNum	: signalMaskList)
			{
				bool mask 					= getbituInc(data, i,	1);
				if (mask)
				{
					RawSig sig;

					sig.code = sigNum;
					E_FType ft = code_to_ftype(rtcmsys, sig.code);

					obs.SigsLists[ft].push_back(sig);

					RawSig* pointer = &obs.SigsLists[ft].back();
					signalPointermap [indx] = pointer;
					cellSatellitemap [indx] = obs.Sat;
					indx++;
				}
			}

			//get satellite specific data - needs to be in chunks
			map<SatSys,bool> SatelliteDatainvalid;
			for (auto& obs : obsList)
			{
				int ms_rough_range			= getbituInc(data, i,	8);
				if (ms_rough_range == 255)
				{
					SatelliteDatainvalid[obs.Sat]=true;
					continue;
				}
				else SatelliteDatainvalid[obs.Sat]=false;
				
				for (auto& [ft, sigList]	: obs.SigsLists)
				for (auto& sig				: sigList)
				{
					sig.P = ms_rough_range;
					sig.L = ms_rough_range;
				}
			}

			map<SatSys,map<E_FType,double>>  GLOFreqShift;
			if(extrainfo)
			{
				for (auto& obs : obsList)
				{
					int extended_sat_info		= getbituInc(data, i,	4);


					if(rtcmsys == +E_Sys::GLO)
					{
						GLOFreqShift[obs.Sat][G1] = 562500.0*(extended_sat_info-7);
						GLOFreqShift[obs.Sat][G2] = 437500.0*(extended_sat_info-7);
 					}
				}
			}
			else if(rtcmsys == +E_Sys::GLO)
			{
				for (auto& obs : obsList)
				{
					short int prn=obs.Sat.prn;
					if(prn>24 || prn<1)
					{
						SatelliteDatainvalid[obs.Sat]=true;
						GLOFreqShift[obs.Sat][G1] = 0.0;
						GLOFreqShift[obs.Sat][G2] = 0.0;
						
					}
					else
					{
						GLOFreqShift[obs.Sat][G1] = 562500.0 * DefGLOChnl[prn-1];
						GLOFreqShift[obs.Sat][G2] = 437500.0 * DefGLOChnl[prn-1];
					}
				}
			}

			for (auto& obs : obsList)
			{
				int rough_range_modulo		= getbituInc(data, i,	10);

				for (auto& [ft, sigList]	: obs.SigsLists)
				for (auto& sig				: sigList)
				{
					sig.P += rough_range_modulo * P2_10;
					sig.L += rough_range_modulo * P2_10;
				}
			}

			if(extrainfo)
			for (auto& obs : obsList)
			{
				int rough_doppler			= getbituInc(data, i,	14);
				if (rough_doppler == 0x2000)
					continue;

				for (auto& [ft, sigList]	: obs.SigsLists)
				for (auto& sig				: sigList)
				{
					sig.D = rough_doppler;
				}
			}

			//get signal specific data

			for (auto& [indx, signalPointer] : signalPointermap)
			{
				int fine_pseudorange		= getbitsInc(data, i,	nbcd);
				if (fine_pseudorange == 0x80000)
				{
					SatelliteDatainvalid[cellSatellitemap[indx]]=true;
					continue;
				}
				
				RawSig& sig = *signalPointer;
				sig.P += fine_pseudorange * sccd;
			}

			for (auto& [indx, signalPointer] : signalPointermap)
			{
				int fine_phase_range		= getbitsInc(data, i,	nbph);
				if (fine_phase_range == 0x800000)
				{
					SatelliteDatainvalid[cellSatellitemap[indx]]=true;
					continue;
				}
				
				RawSig& sig = *signalPointer;
				sig.L += fine_phase_range * scph;
			}

			for (auto& [indx, signalPointer] : signalPointermap)
			{
				int lock_time_indicator	= getbituInc(data, i,	nblk);

				RawSig& sig = *signalPointer;
				sig.LLI=0;
				
				SatSys sat = cellSatellitemap [indx];
				if(  lock_time.find(sat)!=lock_time.end()
				  && lock_time[sat].find(sig.code)!=lock_time[sat].end())
				{
				  	int past_time = lock_time[sat][sig.code];
				  	if(lock_time_indicator<past_time) sig.LLI=1;
				}
				lock_time[sat][sig.code]=lock_time_indicator;
			}

			for (auto& [indx, signalPointer] : signalPointermap)
			{
				int half_cycle_ambiguity	= getbituInc(data, i,	1);

				RawSig& sig = *signalPointer;

			}

			for (auto& [indx, signalPointer] : signalPointermap)
			{
				int carrier_noise_ratio		= getbituInc(data, i,	nbcn);

				RawSig& sig = *signalPointer;
				sig.snr = carrier_noise_ratio * scsn;
			}

			if(extrainfo)
			for (auto& [indx, signalPointer] : signalPointermap)
			{
				int fine_doppler		= getbitsInc(data, i,	15);
				if (fine_doppler == 0x4000)
					continue;

				RawSig& sig = *signalPointer;
				sig.D += fine_doppler			* 0.0001;
			}


			//convert millisecond measurements to meters or cycles
			for (auto& obs				: obsList)
			for (auto& [ft, sigList]	: obs.SigsLists)
			for (auto& sig				: sigList)
			{
				double freqcy = signal_phase_alignment[rtcmsys][ft];
				if(rtcmsys == +E_Sys::GLO) freqcy += GLOFreqShift[obs.Sat][ft];
				
				sig.P *= CLIGHT	/ 1000;
				sig.L *= freqcy	/ 1000;
				
				tracepdeex(rtcmdeblvl+1,std::cout, "\n#RTCM_DEC MSMOBS %s %s %d %s %.4f %.4f", obs.time.to_string(2), obs.Sat.id(), ft, sig.code._to_string(),sig.P, sig.L*CLIGHT/freqcy );
			}

			return obsList;
		}
	};

    static uint16_t message_length(char header[2])
	{
        // Message length is 10 bits starting at bit 6
        return getbitu((uint8_t*)header, 6,	10);
    }

	static RtcmMessageType message_type(const uint8_t message[])
	{
		auto id = getbitu(message, 0,	12);

		if (!RtcmMessageType::_is_valid(id))
		{
			return RtcmMessageType::NONE;
		}

		return RtcmMessageType::_from_integral(id);
	}
};


void RtcmStream::parseRTCM(std::istream& inputStream)
{
	while (inputStream)
	{
        
		int pos;
		while (true)
		{
			// Skip to the start of the frame - marked by preamble character 0xD3
			pos = inputStream.tellg();
			char c = inputStream.get();
			if (inputStream)
			{
				if (c == PREAMBLE)
				{
					break;
				}
			}
			else
			{                
				return;
			}
		}

		// Read the frame length - 2 bytes big endian only want 10 bits
		char buf[2];
		inputStream.read((char*)buf, 2);
		int pos2=inputStream.tellg();
		if (inputStream.fail())
		{
			inputStream.clear();
			inputStream.seekg(pos);
			return;
		}
		
        auto message_length = RtcmDecoder::message_length(buf);

		// Read the frame data (include the header)
        unsigned char data[message_length + 3]; 
		data[0] = PREAMBLE;
		data[1] = buf[0];
		data[2] = buf[1];
		unsigned char* message = data + 3;
		inputStream.read((char*)message, message_length);
		if (inputStream.fail())
		{
			//tracepdeex(rtcmdeblvl+1,std::cout, "\nSTR_%s bytes in buffer", station.name);
			inputStream.clear();
			inputStream.seekg(pos);
			return;
		}
		
		// Read the frame CRC
		unsigned int crcRead = 0;
		inputStream.read((char*)&crcRead, 3);
		if (inputStream.fail())
		{
			//tracepdeex(rtcmdeblvl+1,std::cout, "\nSTR_%s leaving %d bytes in buffer", station.name, message_length);
			inputStream.clear();
			inputStream.seekg(pos);
			return;
		}

		unsigned int crcCalc = crc24q(data, sizeof(data));
		int nmeass = 0;
		if(message_length>8) nmeass = getbitu(message, 0,	12);
		
		if	( (((char*)&crcCalc)[0] != ((char*)&crcRead)[2])
			||(((char*)&crcCalc)[1] != ((char*)&crcRead)[1])
			||(((char*)&crcCalc)[2] != ((char*)&crcRead)[0]))
		{
			//tracepdeex(rtcmdeblvl,std::cout, "\n#RTCM CRC Error for receiver %s %4d\n", station.name, nmeass);
			inputStream.seekg(pos2);
			continue;
		}
 
        //tracepdeex(rtcmdeblvl+1,std::cout, "STR_%s : RTCM Message %4d\n", station.name, nmeass);
        
        
		auto message_type = RtcmDecoder::message_type((unsigned char*) message);

		if 		( message_type == +RtcmMessageType::GPS_EPHEMERIS
				||message_type == +RtcmMessageType::GAL_FNAV_EPHEMERIS
				||message_type == +RtcmMessageType::GAL_INAV_EPHEMERIS)
		{
			RtcmDecoder::EphemerisDecoder decoder;
			decoder.data = (uint8_t*) message;
			decoder.message_length	= message_length;

			decoder.decode();
		}
		else if ( message_type == +RtcmMessageType::GPS_SSR_COMB_CORR
				||message_type == +RtcmMessageType::GPS_SSR_ORB_CORR
				||message_type == +RtcmMessageType::GPS_SSR_CLK_CORR
				||message_type == +RtcmMessageType::GPS_SSR_URA
				||message_type == +RtcmMessageType::GAL_SSR_COMB_CORR
				||message_type == +RtcmMessageType::GAL_SSR_ORB_CORR
				||message_type == +RtcmMessageType::GAL_SSR_CLK_CORR
                ||message_type == +RtcmMessageType::GPS_SSR_CODE_BIAS
				||message_type == +RtcmMessageType::GAL_SSR_CODE_BIAS
				||message_type == +RtcmMessageType::GPS_SSR_PHASE_BIAS
				||message_type == +RtcmMessageType::GAL_SSR_PHASE_BIAS)
		{
			RtcmDecoder::SSRDecoder decoder;
			decoder.data = (uint8_t*) message;
			decoder.message_length	= message_length;

			decoder.decode();
		}
		else if ( message_type == +RtcmMessageType::MSM4_GPS
				||message_type == +RtcmMessageType::MSM4_GLONASS
				||message_type == +RtcmMessageType::MSM4_GALILEO
				||message_type == +RtcmMessageType::MSM4_QZSS
				||message_type == +RtcmMessageType::MSM4_BEIDOU
				||message_type == +RtcmMessageType::MSM5_GPS
				||message_type == +RtcmMessageType::MSM5_GLONASS
				||message_type == +RtcmMessageType::MSM5_GALILEO
				||message_type == +RtcmMessageType::MSM5_QZSS
				||message_type == +RtcmMessageType::MSM5_BEIDOU
				||message_type == +RtcmMessageType::MSM6_GPS
				||message_type == +RtcmMessageType::MSM6_GLONASS
				||message_type == +RtcmMessageType::MSM6_GALILEO
				||message_type == +RtcmMessageType::MSM6_QZSS
				||message_type == +RtcmMessageType::MSM6_BEIDOU
				||message_type == +RtcmMessageType::MSM7_GPS
				||message_type == +RtcmMessageType::MSM7_GLONASS
				||message_type == +RtcmMessageType::MSM7_GALILEO
				||message_type == +RtcmMessageType::MSM7_QZSS
				||message_type == +RtcmMessageType::MSM7_BEIDOU)
		{
			RtcmDecoder::MSMDecoder decoder;
			decoder.data = (uint8_t*) message;
			decoder.message_length	= message_length;

			ObsList obsList = decoder.decode(MSM_lock_time);
			
			int i=54;
			int multimessage = getbituInc(message, i,	1);
			
			//tracepdeex(rtcmdeblvl,std::cout, "\n%s %4d %s %2d %1d", station.name, message_type._to_integral(), obsList.front().time.to_string(0), obsList.size(), multimessage);
			
			if(multimessage==0)
			{
				SuperList.insert(SuperList.end(),obsList.begin(),obsList.end());
				obsListList.push_back(SuperList);
				SuperList.clear();
			}
			else if(SuperList.size()>0
				 && obsList.size()>0	
				 && fabs(timediff(SuperList.front().time,obsList.front().time))>0.5)
			{
				obsListList.push_back(SuperList);
				SuperList.clear();
				SuperList.insert(SuperList.end(),obsList.begin(),obsList.end());
			}
			else
				SuperList.insert(SuperList.end(),obsList.begin(),obsList.end());
				
			if(SuperList.size()>1000) SuperList.clear();
		}
	} 	
}

void	initSsrOut(double tgap)
{
	if (udiMap.find(acsConfig.ssrOpts.update_interval) == udiMap.end())
	{
		BOOST_LOG_TRIVIAL(error) << "Error: Config ssrOpts.update_interval is not valid (" << acsConfig.ssrOpts.update_interval << ").";
	}

	int tgapInt = (int)round(tgap);
	if (acsConfig.ssrOpts.update_interval % tgapInt != 0)
	{
		BOOST_LOG_TRIVIAL(error) << "Error: Config ssrOpts.update_interval (" << acsConfig.ssrOpts.update_interval << ") is not a multiple of processing_options.epoch_interval (" << tgap << ").";
	}
	int udiInt = (int)round(acsConfig.ssrOpts.update_interval) / tgapInt;

	for (auto &[key, entry] : nav.satNavMap)
	{
		// (Re)set sat count to zero
		entry.ssrOut.numObs = 0;

		// Set UDI in # epochs
		entry.ssrOut.udiEpochs = udiInt;

		// Set UDI in all structs
		int udiEncoded = udiMap.at(acsConfig.ssrOpts.update_interval);
		entry.ssrOut.ssrEph.udi			= udiEncoded;
		entry.ssrOut.ssrClk.udi			= udiEncoded;
		entry.ssrOut.ssrBias.udi_code	= udiEncoded;
		entry.ssrOut.ssrBias.udi_phas	= udiEncoded;
	}
}

void	calcSsrOut()
{
	using namespace std; //todo mark: remove
	for (auto& [key, entry] : nav.satNavMap)
	{
		if (entry.ssrOut.epochsSinceUpdate >= entry.ssrOut.udiEpochs)
		{
			//Export
			//todo mark - export entry.ssrOut to file
			if (entry.ssrOut.ssrClk.preciseIsSet &&
				entry.ssrOut.ssrClk.broadcastIsSet)
			{
				//if (entry.ssrOut.prevDiffIsSet)
				double offsetDiff					= entry.ssrOut.ssrClk.precise - entry.ssrOut.ssrClk.broadcast;
				entry.ssrOut.lock();
				entry.ssrOut.ssrClk.dclk[0]			= offsetDiff;
				entry.ssrOut.ssrClk.dclk[1]			= 0;	// set to zero (not used)
				entry.ssrOut.ssrClk.dclk[2]			= 0;	// set to zero (not used)
				entry.ssrOut.ssrClk.readyForExport	= true; //todo mark: set to false when exported
				entry.ssrOut.unlock();
				
				//entry.ssrOut.prevDiff = offsetDiff;
				//entry.ssrOut.prevDiffIsSet = true;
				cout << entry.ssrOut.ssrClk.precise << " " << entry.ssrOut.ssrClk.broadcast << " " << entry.ssrOut.ssrClk.dclk[0] << endl;
			}
			entry.ssrOut.ssrClk.preciseIsSet 		= false;
			entry.ssrOut.ssrClk.broadcastIsSet 	= false;

			entry.ssrOut.epochsSinceUpdate = 0;
		}
		else
		{
			++entry.ssrOut.epochsSinceUpdate;
		}
	}
}

void	exportSsrOut()
{
	using namespace std; //todo mark: remove
	std::ofstream out;
	out.open("ssrOut.dbg", std::ios::app);
	
	// Header
	out << "sEpochNum" 			<< "\t";
	out << "SSREph.t0"			<< "\t";
	out << "SSREph.udi"			<< "\t";
	out << "SSREph.iod"			<< "\t";
	out << "SSREph.iode"		<< "\t";
	out << "SSREph.deph[0]"		<< "\t";
	out << "SSREph.deph[1]"		<< "\t";
	out << "SSREph.deph[2]"		<< "\t";
	out << "SSREph.ddeph[0]"	<< "\t";
	out << "SSREph.ddeph[1]"	<< "\t";
	out << "SSREph.ddeph[2]"	<< "\t";

	out << "SSRClk.t0"			<< "\t";
	out << "SSRClk.udi"			<< "\t";
	out << "SSRClk.iod"			<< "\t";
	out << "SSRClk.dclk[0]"		<< "\t";
	out << "SSRClk.dclk[1]"		<< "\t";
	out << "SSRClk.dclk[2]"		<< "\t";

	out << "SSRBias.t0_code"	<< "\t";
	out << "SSRBias.t0_phas"	<< "\t";
	out << "SSRBias.udi_code"	<< "\t";
	out << "SSRBias.udi_phas"	<< "\t";
	out << "SSRBias.iod_code"	<< "\t";
	out << "SSRBias.iod_phas"	<< "\t";
	for (auto& [key, val] : nav.satNavMap.begin()->second.ssrOut.ssrBias.cbias)	out << "ssrEph.cbias"<< "\t";
	for (auto& [key, val] : nav.satNavMap.begin()->second.ssrOut.ssrBias.cvari)	out << "ssrEph.cvari"<< "\t";
	for (auto& [key, val] : nav.satNavMap.begin()->second.ssrOut.ssrBias.pbias)	out << "ssrEph.pbias"<< "\t";
	for (auto& [key, val] : nav.satNavMap.begin()->second.ssrOut.ssrBias.pvari)	out << "ssrEph.pvari"<< "\t";
	out << endl;

	// Body
	for (auto& [key, entry] : nav.satNavMap)
	{
		if (entry.ssrOut.epochsSinceUpdate >= entry.ssrOut.udiEpochs)
		{
			out << sEpochNum << "\t";
			out << entry.ssrOut.ssrEph.t0		<< "\t";
			out << entry.ssrOut.ssrEph.udi		<< "\t";
			out << entry.ssrOut.ssrEph.iod		<< "\t";
			out << entry.ssrOut.ssrEph.iode		<< "\t";
			out << entry.ssrOut.ssrEph.deph[0]	<< "\t";
			out << entry.ssrOut.ssrEph.deph[1]	<< "\t";
			out << entry.ssrOut.ssrEph.deph[2]	<< "\t";
			out << entry.ssrOut.ssrEph.ddeph[0]	<< "\t";
			out << entry.ssrOut.ssrEph.ddeph[1]	<< "\t";
			out << entry.ssrOut.ssrEph.ddeph[2]	<< "\t";

			out << entry.ssrOut.ssrClk.t0		<< "\t";
			out << entry.ssrOut.ssrClk.udi		<< "\t";
			out << entry.ssrOut.ssrClk.iod		<< "\t";
			out << entry.ssrOut.ssrClk.dclk[0]	<< "\t";
			out << entry.ssrOut.ssrClk.dclk[1]	<< "\t";
			out << entry.ssrOut.ssrClk.dclk[2]	<< "\t";

			out << entry.ssrOut.ssrBias.t0_code	<< "\t";
			out << entry.ssrOut.ssrBias.t0_phas	<< "\t";
			out << entry.ssrOut.ssrBias.udi_code<< "\t";
			out << entry.ssrOut.ssrBias.udi_phas<< "\t";
			out << entry.ssrOut.ssrBias.iod_code<< "\t";
			out << entry.ssrOut.ssrBias.iod_phas<< "\t";
			for (auto& [key, val] : entry.ssrOut.ssrBias.cbias)	out << val<< "\t";
			for (auto& [key, val] : entry.ssrOut.ssrBias.cvari)	out << val<< "\t";
			for (auto& [key, val] : entry.ssrOut.ssrBias.pbias)	out << val<< "\t";
			for (auto& [key, val] : entry.ssrOut.ssrBias.pvari)	out << val<< "\t";
			out << endl;
		}
	}
	out.close();
	++sEpochNum;
}

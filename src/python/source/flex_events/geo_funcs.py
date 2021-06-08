'''

Functions used to help with geographical operations
So far this module includes:
- WGS84 constants
- Coordinate transformations

Resources used:
https://en.wikipedia.org/wiki/Geographic_coordinate_conversion#From_ECEF_to_ENU
https://github.com/kristinemlarson/gnssIR_python/gps.py

Author: Ronald Maj 
2020-10-21
'''

# import georinex as gr
#%run obs_code_plot.py 'MDO100USA' '2018' '117' 'S2W' 'ALL' '01:45' '2:15'
#sp3_f = gr.load('igs19985.sp3')

#gps_sats = [x for x in obs.sv.values if 'G' in x]
#gps_sp3 = sp3_f.sel(sv = gps_sats)

#com_times = [x for x in gps_sp3.time.values if x in obs.time.values]

#com_obs = obs.sel(time = com_times).sel(sv = gps_sats)
#com_sp3 = gps_sp3.sel(time = com_times)

from pathlib import Path
import sys

sys.path.append(str(Path.cwd()))
from gn_lib.gn_transform import llh2rot, xyz2llh_heik, xyz2llh_zhu, xyz2llh_larson

import numpy as np
from numpy import sin,cos
from numpy.core.umath_tests import inner1d



# ###### Taken from Kristen Larson's code on calculating reflectometry ########
# ######    https://github.com/kristinemlarson/gnssIR_python/gps.py    ########

class wgs84:
    """
    wgs84 parameters
    """
    a = 6378137. # meters
    f  =  1./298.257223563 # flattening factor
    e = np.sqrt(2*f-f**2) # 


def xyz2llh(xyz, tol):
    """
    inputs are station coordinate vector xyz (x,y,z in meters), tolerance for convergence
    outputs are lat, lon in radians and wgs84 ellipsoidal height in meters
    kristine larson
    """
    x = xyz[0]
    y = xyz[1]
    z = xyz[2]
    
    lon = np.arctan2(y, x)
    
    p = np.sqrt(x**2+y**2)
    lat0 = np.arctan((z/p)/(1-wgs84.e**2))
    
    b = wgs84.a*(1-wgs84.f)
    error = 1
    
    a2=wgs84.a**2
    
    i=0 # make sure it doesn't go forever
    
    while error > tol and i < 10:
    
        n = a2/np.sqrt(a2*np.cos(lat0)**2+b**2*np.sin(lat0)**2)
        h = p/np.cos(lat0)-n
    
        lat = np.arctan((z/p)/(1-wgs84.e**2*n/(n+h)))
    
        error = np.abs(lat-lat0)
        lat0 = lat
        i+=1
    
    return lat, lon, h
# #######################################################################
# #######################################################################


# # Now need to calculate the conversion matrix and convert from ECEF to ENU coords
# # I'll use the wikipedia definition:
# # https://en.wikipedia.org/wiki/Geographic_coordinate_conversion#From_ECEF_to_ENU

def make_array(rec_pos, sat_pos):
    '''
    Ensure that the inputs are arrays, if list convert to np.arrays
    '''
    # If either rec_pos or sat_pos are lists, change list to np array 
    if type(rec_pos) == list:
        rec_pos = np.array(rec_pos)
    if type(sat_pos) == list:
        sat_pos = np.array(sat_pos)
    return rec_pos, sat_pos



def ecef2enu(rec_pos, sat_pos):
    ''' 
    Convert satellite positions from ECEF to ENU (given the receiver position)

    Input:
    rec_pos - receiver position in ECEF (m)
    sat_pos - satellite position in ECEF (m)

    Output:
    Satellite Position in ENU local coordinates (from receiver perspective)
    '''   
    # If either rec_pos or sat_pos are lists, change list to np array 
    rec_pos, sat_pos = make_array(rec_pos, sat_pos)

    # Ensure coords are correct shape:
    rec_pos = rec_pos.reshape(3,1)
    sat_pos = sat_pos.reshape(3,1)

    # Get the lat,lon,hei (phi,lamb,alt) coordinates of the receiver
    phi, lamb,__ = xyz2llh(rec_pos, 1e-8)

    # Create transformation matrix
    R0 = [-sin(lamb)[0], cos(lamb)[0], 0.0]
    R1 = [-sin(phi)[0]*cos(lamb)[0], -sin(phi)[0]*sin(lamb)[0], cos(phi)[0]]
    R2 = [cos(phi)[0]*cos(lamb)[0], cos(phi)[0]*sin(lamb)[0], sin(phi)[0]]
    R = np.array([R0,R1,R2])
    
    # Carry out transformation
    return np.matmul(R,sat_pos-rec_pos)


def el_ang(rec_pos, sat_pos):
    ''' 
    Calculate the elevation angle from receiver and satellite position

    Input:
    rec_pos - receiver position in ECEF (m)
    sat_pos - satellite position in ECEF (m)

    Output:
    Elevation Angle (rad)
    '''
    # If either rec_pos or sat_pos are lists, change list to np array 
    rec_pos, sat_pos = make_array(rec_pos, sat_pos)
        
    # Convert satellite coords from ECEF to ENU vector
    enu_vec = ecef2enu(rec_pos, sat_pos)
    # Make ENU vector unit length
    enu_bar = enu_vec / np.linalg.norm(enu_vec)
    # Calculate the elevation angle
    return np.arcsin(enu_bar[2])[0]


def calc_dist(rec_pos, sat_pos):
    '''
    Calculate the distance from the receiver to the satellite
    
    Input:
    rec_pos - receiver position in ECEF (m)
    sat_pos - satellite position in ECEF (m)

    Output:
    Distance (m)    
    '''
    # If either rec_pos or sat_pos are lists, change list to np array 
    rec_pos, sat_pos = make_array(rec_pos, sat_pos)

    return np.linalg.norm(rec_pos - sat_pos)


def add_all_angs(
    df_sats_pos,
    station_pos,
    nad=True,
    el=True,
    az=True,
    return_lists=False):
    '''
    Add new columns to dataframe (nad_ang, el_ang and/or az_ang) after 
    calculating nadir, elevation and/or azimuth angles for given site 'station_pos'
    Default is to 
    '''
    
    sat_pos_arr = df_sats_pos[['x','y','z']].to_numpy(dtype=float)
    disp_vec_arr = sat_pos_arr - (station_pos.reshape(1,3)/1000)
    
    nad_angs = []
    el_angs = []
    az_angs = []
    
    if nad:
        a = np.linalg.norm(sat_pos_arr,axis=1)
        b = np.linalg.norm(disp_vec_arr,axis=1)

        df_sats_pos['nad_ang'] = np.arccos(inner1d(sat_pos_arr, disp_vec_arr)/(a*b))*(180/np.pi)    
    
    if el or az:
        station_llh = xyz2llh_heik(station_pos.T)[0]
        R = llh2rot(np.array([station_llh[0]]),np.array([station_llh[1]]))

        enu_pos = np.matmul(R[0],disp_vec_arr.T).T
        enu_bar = enu_pos / np.linalg.norm(enu_pos,axis=1).reshape(len(enu_pos),1)
        
        if el:
            df_sats_pos['el_ang'] = np.arcsin(enu_bar[:,2])*(180/np.pi)
        if az:
            df_sats_pos['az_ang'] = np.arctan2(enu_bar[:,0],enu_bar[:,1])
        
        if return_lists:
            return nad_angs, el_angs, az_angs
import numpy as np
import os
import mbircone
import argparse
import getpass
from psutil import cpu_count
from demo_utils import load_yaml, plt_cmp_3dobj, create_cluster_ticket_configs
from scipy import ndimage

if __name__ == '__main__':
    """
    This script is a demonstration of how to use scatter_gather() to deploy parallel jobs.  Demo functionality includes
     * obatin cluster ticket with get_cluster_ticket().
     Use scatter_gather to:
     * generating 4D simulated data by rotating the 3D shepp logan phantom by increasing degree per time point.
     * generating sinogram data by projecting each phantom in all timepoints.
     * performing 3D reconstruction in all timepoints.
    """
    print('This script is a demonstration of how to use scatter_gather() to deploy parallel jobs.  Demo functionality includes \
    \n\t * obatin cluster ticket with get_cluster_ticket(). \
    \n\t * Use scatter_gather to:\
    \n\t * generating 4D simulated data by rotating the 3D shepp logan phantom by increasing degree per time point. \
    \n\t * generating sinogram data by projecting each phantom in all timepoints. \
    \n\t * performing 3D reconstruction in all timepoints.')

    # ###########################################################################
    # Load the parameters from configuration file to obtain a cluster ticket.
    # ###########################################################################

    # Ask for a configuration file to obtain a cluster ticket to access a parallel cluster.
    # If the configuration file is not provided, it will automatically set up a LocalCluster based on the number of
    # cores in the local computer and return a ticket needed for :py:func:`~multinode.scatter_gather`.
    parser = argparse.ArgumentParser(description='A demo help users use mbircone.multinode module on High Performance Computer.')
    parser.add_argument('--configs_path', type=str, default=None, help="Path to a configuration file.")
    parser.add_argument('--no_multinode', default=False, action='store_true', help="Run this demo in a single node machine.")
    args = parser.parse_args()
    save_config_dir = './configs/multinode/'

    # ###########################################################################
    # Set the parameters to do the recon
    # ###########################################################################

    # sinogram parameters
    num_det_rows = 200
    num_det_channels = 128
    num_views = 144

    # Reconstruction parameters
    sharpness = 0.2
    snr_db = 31.0

    # magnification is unit-less.
    magnification = 2

    # All distances are in unit of 1 ALU = 1 mm.
    dist_source_detector = 600
    delta_pixel_detector = 0.9
    delta_pixel_image = 1.0
    channel_offset = 0
    row_offset = 0
    max_iterations = 20

    # Display parameters
    vmin = 1.0
    vmax = 1.1
    filename = 'output/3D_shepp_logan/results_%d.png'

    # Parallel computer verbose
    par_verbose = 0

    # Number of parallel functions
    num_parallel = 4

    # ###########################################################################
    # Obtain a cluster ticket.
    # ###########################################################################

    # Obtain a ticket(LocalCluster, SLURMCluster, SGECluster) needed for :py:func:`~multinode.scatter_gather` to access a parallel cluster.
    # More information of obtaining this ticket can be found in below webpage.
    # API of dask_jobqueue https://jobqueue.dask.org/en/latest/api.html
    # API of https://docs.dask.org/en/latest/setup/single-distributed.html#localcluster

    if args.no_multinode:
        num_physical_cores = cpu_count(logical=False)
        cluster_ticket = mbircone.multinode.get_cluster_ticket('LocalHost', num_physical_cores_per_node=num_physical_cores)
    else:
        if args.configs_path is None:
            # create output folder
            os.makedirs(save_config_dir, exist_ok=True)
            # Create a configuration dictionary for a cluster ticket, by collecting required information from terminal.
            configs = create_cluster_ticket_configs(save_config_dir=save_config_dir)

        else:
            # Load cluster setup parameter.
            configs = load_yaml(args.configs_path)

        cluster_ticket = mbircone.multinode.get_cluster_ticket(
            job_queue_system_type=configs['job_queue_system_type'],
            num_physical_cores_per_node=configs['cluster_params']['num_physical_cores_per_node'],
            num_nodes=configs['cluster_params']['num_nodes'],
            maximum_memory_per_node=configs['cluster_params']['maximum_memory_per_node'],
            maximum_allowable_walltime=configs['cluster_params']['maximum_allowable_walltime'],
            system_specific_args=configs['cluster_params']['system_specific_args'],
            local_directory=configs['cluster_params']['local_directory'],
            log_directory=configs['cluster_params']['log_directory'])


    print(cluster_ticket)
    print("Parallel computing 3D conebeam reconstruction at %d timepoints.\n" % num_parallel)

    # ###########################################################################
    # Generate a 3D shepp logan phantom.
    # ###########################################################################

    ROR, boundary_size = mbircone.cone3D.compute_img_size(num_views, num_det_rows, num_det_channels,
                                                          dist_source_detector,
                                                          magnification,
                                                          channel_offset=channel_offset, row_offset=row_offset,
                                                          delta_pixel_detector=delta_pixel_detector,
                                                          delta_pixel_image=delta_pixel_image)
    Nz, Nx, Ny = ROR
    img_slices_boundary_size, img_rows_boundary_size, img_cols_boundary_size = boundary_size
    print('Region of reconstruction (ROR) shape is:', (Nz, Nx, Ny))

    # Set phantom parameters to generate a phantom inside ROI according to ROR and boundary_size.
    # All valid pixels should be inside ROI.
    num_rows_cols = Nx - 2 * img_rows_boundary_size  # Assumes a square image
    num_slices_phantom = Nz - 2 * img_slices_boundary_size
    print('Region of interest (ROI) shape is:', num_slices_phantom, num_rows_cols, num_rows_cols)

    # Set display indexes
    display_slice = img_slices_boundary_size + int(0.4 * num_slices_phantom)
    display_x = num_rows_cols // 2
    display_y = num_rows_cols // 2
    display_view = 0

    # Generate a 3D phantom
    phantom = mbircone.phantom.gen_shepp_logan_3d(num_rows_cols, num_rows_cols, num_slices_phantom)
    print('Generated 3D phantom shape before padding = ', np.shape(phantom))
    phantom = mbircone.cone3D.pad_roi2ror(phantom, boundary_size)
    print('Padded 3D phantom shape = ', np.shape(phantom))
    print()

    # ###########################################################################
    # Generate a 4D shepp logan phantom.
    # ###########################################################################

    print("Generating 4D simulated data by rotating the 3D shepp logan phantom with positive angular steps with each time point ...")
    # Generate 4D simulated data by rotating the 3D shepp logan phantom by increasing degree per time point.
    # Create the rotation angles and argument lists, and distribute to workers.
    phantom_rot_para = np.linspace(0, 180, num_parallel, endpoint=False)  # Phantom rotation angles.
    phantom_list = [ndimage.rotate(input=phantom,
                                   angle=phantom_rot_ang,
                                   order=0,
                                   mode='constant',
                                   axes=(1, 2),
                                   reshape=False) for phantom_rot_ang in phantom_rot_para]
    print()

    # ###########################################################################
    # Generate sinogram
    # ###########################################################################

    print("****Multinode computation with Dask****: Generating sinogram data by projecting each phantom at each timepoint ...")
    # scatter_gather parallel computes mbircone.cone3D.project
    # Generate sinogram data by projecting each phantom in phantom list.
    # Create the projection angles and argument lists, and distribute to workers.
    proj_angles = np.linspace(0, 2 * np.pi, num_views, endpoint=False)  # Same for all time points.
    # After setting the geometric parameter, the shape of the input phantom should be equal to the calculated
    # geometric parameter. Input a phantom with wrong shape will generate a bunch of issue in C.
    variable_args_list = [{'image': phantom_rot} for phantom_rot in phantom_list]
    constant_args = {'angles': proj_angles,
                     'num_det_rows': num_det_rows,
                     'num_det_channels': num_det_channels,
                     'dist_source_detector': dist_source_detector,
                     'magnification': magnification,
                     'delta_pixel_detector': delta_pixel_detector,
                     'delta_pixel_image': delta_pixel_image,
                     'channel_offset': channel_offset,
                     'row_offset': row_offset}
    sino_list = mbircone.multinode.scatter_gather(cluster_ticket,
                                                  mbircone.cone3D.project,
                                                  constant_args=constant_args,
                                                  variable_args_list=variable_args_list,

                                                  verbose=par_verbose)
    print()

    # ###########################################################################
    # Perform multinode reconstruction
    # ###########################################################################

    print("****Multinode computation with Dask****: Reconstructing 3D phantom at all timepoints ...")
    # scatter_gather parallel computes mbircone.cone3D.recon
    # Reconstruct 3D phantom in all timepoints using mbircone.cone3D.recon.
    # Create the projection angles and argument lists, and distribute to workers.
    angles_list = [np.copy(proj_angles) for i in range(num_parallel)]  # Same for all time points.
    variable_args_list = [{'sino': sino, 'angles': angles} for sino, angles in zip(sino_list, angles_list)]
    constant_args = {'dist_source_detector': dist_source_detector,
                     'magnification': magnification,
                     'delta_pixel_detector': delta_pixel_detector,
                     'delta_pixel_image': delta_pixel_image,
                     'channel_offset': channel_offset,
                     'row_offset': row_offset,
                     'sharpness': sharpness,
                     'snr_db': snr_db,
                     'max_iterations': max_iterations,
                     'verbose': 0}
    recon_list = mbircone.multinode.scatter_gather(cluster_ticket,
                                                   mbircone.cone3D.recon,
                                                   constant_args=constant_args,
                                                   variable_args_list=variable_args_list,
                                                   verbose=par_verbose)

    print("Reconstructed 4D image shape = ", np.array(recon_list).shape)

    # create output folder and save reconstruction list
    os.makedirs('output/3D_shepp_logan/', exist_ok=True)
    np.save("./output/3D_shepp_logan/recon_sh4d.npy", np.array(recon_list))

    # Display and compare reconstruction
    for i in range(num_parallel):
        plt_cmp_3dobj(phantom_list[i], recon_list[i], display_slice, display_x, display_y, vmin, vmax, filename % i)
    input("press Enter")

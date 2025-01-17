
#include "computeSysMatrix.h"

 

void computeSysMatrix(struct SinoParams *sinoParams, struct ImageParams *imgParams, struct SysMatrix *A, struct ViewAngleList *viewAngleList)
{
    float ticToc;
    tic(&ticToc);
    
    // printf("\nInitialize Sinogram Mask ...\n");
    // printf("\nCompute SysMatrix Parameters...\n");

    computeAMatrixParameters(sinoParams, imgParams, A, viewAngleList);
    allocateSysMatrix(A, imgParams->N_x, imgParams->N_y, imgParams->N_z, sinoParams->N_beta, A->i_vstride_max, A->i_wstride_max, A->N_u);
   
    // printf("\nPrecompute B...\n");
    computeBMatrix( sinoParams, imgParams, A, viewAngleList);

    
    // printf("\nPrecompute C...\n");
    computeCMatrix(sinoParams, imgParams, A);


    toc(&ticToc);
    ticTocDisp(ticToc, "computeSysMatrix");
}

/* Paper referenced is Balke et al "Separable Models for cone-beam MBIR Reconstruction" */

void computeAMatrixParameters(struct SinoParams *sinoParams, struct ImageParams *imgParams, struct SysMatrix *A, struct ViewAngleList *viewAngleList)
{
    /* Part 1: Find i_vstride_max, u_0, u_1 */
    float x_v, y_v;
    float u_v, v_v, w_v;
    float beta, alpha_xy, theta;
    float cosine, sine;
    float W_pv, W_pw, M;
    long int j_x, j_y, i_beta, i_vstart, i_vstride, i_wstart, i_wstride, j_u, j_z;


    long int i_vstride_max = 0, i_wstride_max = 0;
    float u_0 = INFINITY;
    float u_1 = -INFINITY;

    float B_ij_max = 0;
    float C_ij_max = 0;
    float delta_v;
    float delta_w;
    float L_v;
    float L_w;
    int temp_stop;

    
    for (j_x = 0; j_x <= imgParams->N_x-1; ++j_x)
    {
        for (j_y = 0; j_y <= imgParams->N_y-1; ++j_y)
        {
            /* retrieve (x_v, y_v) voxel center position in image coordinates */
            x_v = j_x * imgParams->Delta_xy + (imgParams->x_0 + imgParams->Delta_xy/2);
            y_v = j_y * imgParams->Delta_xy + (imgParams->y_0 + imgParams->Delta_xy/2);
            
            
            for (i_beta = 0; i_beta <= sinoParams->N_beta-1 ; ++i_beta)
            {
                beta = viewAngleList->beta[i_beta];

                /* calculate (u_v, v_v) voxel center position in scanner coordinates */
                /* accomplished by "rotating around" the position as a detector would */
                /* for cone3D.py, u_r = 0 */
                cosine = cos(beta);
                sine = sin(beta);
                u_v = cosine * x_v - sine * y_v + sinoParams->u_r;
                v_v = sine * x_v + cosine * y_v + sinoParams->v_r;

                /* calculate magnification factor as a result of projection */
                /* M = (dist source detector) / (dist voxel detector) */
                M = (sinoParams->u_d0 - sinoParams->u_s) / (u_v - sinoParams->u_s);
                
                /* triangle: (v_v, u_v) voxel center, (0, u_s) source, source-detector line */
                theta = atan2(v_v, u_v - sinoParams->u_s );
                
                /* beta = view angle, theta = angle voxel makes with source-detector line */
                /* see Figure 1 in Balke et al  */
                /* alpha = pi/2 + theta - beta is technically the right value */
                /* alpha mod pi/2 = theta - beta */
                /* but cos is an even function so sign doesn't matter */
                alpha_xy = beta - theta;
                alpha_xy = fmod(alpha_xy + PI/4, PI/2) - PI/4;
                W_pv = M * imgParams->Delta_xy * cos(alpha_xy) / cos(theta);

                /* compute start coordinate of voxel footprint in detector index */
                /* M*v_v = center of voxel footprint on detector, image coords */
                /* M*v_v - W_pv/2 = start of voxel footprint on detector, image coords */
                /* v_d0 + Delta_dv/2 = center of first detector box */
                i_vstart = round( (M*v_v - W_pv/2 - (sinoParams->v_d0 + sinoParams->Delta_dv/2))/ sinoParams->Delta_dv ) ;
                i_vstart = _MAX_(0, i_vstart);

                /* compute end coordinate of voxel footprint in detector index */
                /* same logic as above, with M*v_v + W_pv/2 = end of voxel footprint */
                temp_stop =  round( (M*v_v + W_pv/2 - (sinoParams->v_d0 + sinoParams->Delta_dv/2))/ sinoParams->Delta_dv );
                temp_stop = _MIN_(temp_stop, sinoParams->N_dv-1);

                /* voxel footprint width in array indices */
                i_vstride = _MAX_(temp_stop - i_vstart + 1, 0);

                /* update largest so-far voxel width  */
                i_vstride_max = _MAX_(i_vstride, i_vstride_max);
                
                /* update min and max coordinates of possible image voxels */
                u_0 = _MIN_((u_v-imgParams->Delta_xy/2), u_0);
                u_1 = _MAX_((u_v-imgParams->Delta_xy/2), u_1);

                /* calculate B_ij according to (6) (7) (8) (9) (10) */
                /* but only store the max so far B_ij_max */
                #if ISBIJCOMPRESSED == 1
                    cosine = cos(alpha_xy);
                    delta_v = 0;
                    L_v = (W_pv - sinoParams->Delta_dv)/2;           /* b                         */
                    L_v = _ABS_(L_v);                                /* |b|                         */
                    L_v = _MAX_(L_v, delta_v);                        /* max(|b|, c)                 */
                    L_v = (W_pv + sinoParams->Delta_dv)/2 - L_v;    /* a - max(|b|, c)             */
                    L_v = _MAX_(L_v, 0);                            /* max{ a - max(|b|, c), 0} */

                    B_ij_max = _MAX_(imgParams->Delta_xy * L_v / (cosine * sinoParams->Delta_dv), B_ij_max);
                #endif

            }
        }
    }
    A->i_vstride_max = i_vstride_max;
    A->u_0 = u_0;
    A->u_1 = u_1;
    A->B_ij_max = B_ij_max;
    #if ISBIJCOMPRESSED == 1
        A->B_ij_scaler = B_ij_max / 255;
    #else
        A->B_ij_scaler = 1;
    #endif

    /* Compute resulting struct SysMatrix parameters from these */
    A->Delta_u = imgParams->Delta_xy / AMATRIX_RHO;
    A->N_u = ceil((A->u_1 - A->u_0) / A->Delta_u) + 1;

    A->u_1 = u_0 + A->N_u * A->Delta_u;     /* Find most accurate value of u_1 */


    /* Part 2: Find i_wstride_max */
    /* iterate over image voxels */
    for (j_u = 0; j_u <= A->N_u-1; ++j_u)
    {
        /* retrieve voxel center in image coordinates, u coordinate */
        u_v = j_u * A->Delta_u + (A->u_0 + imgParams->Delta_xy/2);
        
        /* magnification */
        M = (sinoParams->u_d0 - sinoParams->u_s) / (u_v - sinoParams->u_s);
        
        /* size of flattened voxel footprint on detector */
        W_pw = M * imgParams->Delta_z;

        for (j_z = 0; j_z <= imgParams->N_z-1; ++j_z)
        {
            /* w_v = voxel center in image coordinates, w coordinate (height) */
            w_v = j_z * imgParams->Delta_z + (imgParams->z_0 + imgParams->Delta_z/2);
            
            /* compute start coordinate of voxel footprint in detector index */
            i_wstart = (M * w_v - (sinoParams->w_d0 + sinoParams->Delta_dw/2) - W_pw/2 ) * (1/sinoParams->Delta_dw) + 0.5;
            i_wstart = _MAX_(i_wstart, 0);

            /* compute end coordinate of voxel footprint in detector index */
            temp_stop = (M * w_v - (sinoParams->w_d0 + sinoParams->Delta_dw/2) + W_pw/2 ) * (1/sinoParams->Delta_dw) + 0.5;
            temp_stop = _MIN_(temp_stop, sinoParams->N_dw-1);

            i_wstride = _MAX_(temp_stop - i_wstart + 1, 0);

            i_wstride_max = _MAX_(i_wstride, i_wstride_max);
            
            /* compute C_ij according to (11) (12) */
            /* track the largest in C_ij_max */
            #if ISCIJCOMPRESSED == 1
                delta_w = 0;
                
                /* eq (12) */
                /* L_w = max{a - max{|b|, c}, 0} */
                L_w = (W_pw - sinoParams->Delta_dw)/2;            /* b */
                L_w = _ABS_(L_w);                                /* |b| */
                L_w = _MAX_(L_w, delta_w);                        /* max{|b|, c} */
                L_w = (W_pw + sinoParams->Delta_dw)/2 - L_w;    /* a - max{|b|, c} */
                L_w = _MAX_(L_w, 0);                            /* max{a - max{|b|, c}, 0} */
                
                /* eq (11) */
                /* see figure 2: alpha = phi */
                /* 1/cos(alpha) = sqrt( 1 + ( w_v/(u_v-u_s) )^2 ) */
                C_ij_max = _MAX_((1/sinoParams->Delta_dw) * sqrt( 1 + (w_v*w_v)/((u_v-sinoParams->u_s)*(u_v-sinoParams->u_s)) ) * L_w, C_ij_max);
            #endif
        }
    }
    
    A->i_wstride_max = i_wstride_max;
    A->C_ij_max = C_ij_max;
    A->C_ij_scaler = C_ij_max / 255;
    
    #if ISCIJCOMPRESSED == 1
        A->C_ij_scaler = C_ij_max / 255;
    #else
        A->C_ij_scaler = 1;
    #endif

}


void computeBMatrix(struct SinoParams *sinoParams, struct ImageParams *imgParams, struct SysMatrix *A, struct ViewAngleList *viewAngleList)
{
    /* Variable declarations */
    float x_v, y_v;
    float u_v, v_v;
    float beta, theta, alpha_xy;
    float cosine, sine;
    float W_pv, M;
    float v_d;
    float delta_v;
    float L_v;
    float B_ij;
    float temp_stop;

    long int j_x, j_y, i_beta, i_v;


    for (j_x = 0; j_x <= imgParams->N_x-1; ++j_x)
    {
        for (j_y = 0; j_y <= imgParams->N_y-1; ++j_y)
        {

            /* Function Body */
            x_v = j_x * imgParams->Delta_xy + (imgParams->x_0 + imgParams->Delta_xy/2);
            y_v = j_y * imgParams->Delta_xy + (imgParams->y_0 + imgParams->Delta_xy/2);

            for (i_beta = 0; i_beta <= sinoParams->N_beta-1 ; ++i_beta)
            {
                /* Calculate i_vstart, i_vstride and i_vstride_max */
                beta = viewAngleList->beta[i_beta];

                /* calculate (u_v, v_v) voxel center position in scanner coordinates */
                /* accomplished by "rotating around" the position as a detector would */
                /* for cone3D.py, u_r = 0 */
                cosine = cos(beta);
                sine = sin(beta);
                u_v = cosine * x_v - sine * y_v + sinoParams->u_r;
                v_v = sine * x_v + cosine * y_v + sinoParams->v_r;

                /* calculate magnification factor as a result of projection */
                /* M = (dist source detector) / (dist voxel detector) */
                M = (sinoParams->u_d0 - sinoParams->u_s) / (u_v - sinoParams->u_s);

                /* triangle: (v_v, u_v) voxel center, (0, u_s) source, source-detector line */
                theta = atan2(v_v, u_v - sinoParams->u_s );
                
                /* beta = view angle, theta = angle voxel makes with source-detector line */
                /* see Figure 1 in Balke et al  */
                /* alpha = pi/2 + theta - beta is technically the right value */
                /* alpha mod pi/2 = theta - beta */
                /* but cos is an even function so sign doesn't matter */
                alpha_xy = beta - theta;
                alpha_xy = fmod(alpha_xy + PI/4, PI/2) - PI/4;
                W_pv = M * imgParams->Delta_xy * cos(alpha_xy) / cos(theta);

                /* compute start coordinate of voxel footprint in detector index */
                /* M*v_v = center of voxel footprint on detector, image coords */
                /* M*v_v - W_pv/2 = start of voxel footprint on detector, image coords */
                /* v_d0 + Delta_dv/2 = center of first detector box */
                A->i_vstart[j_x][j_y][i_beta] = (M*v_v - W_pv/2 - (sinoParams->v_d0 + sinoParams->Delta_dv/2))/ sinoParams->Delta_dv + 0.5;
                A->i_vstart[j_x][j_y][i_beta] = _MAX_(A->i_vstart[j_x][j_y][i_beta], 0);
                
                /* compute end coordinate of voxel footprint in detector index */
                /* same logic as above, with M*v_v + W_pv/2 = end of voxel footprint */
                temp_stop =  (M*v_v + W_pv/2 - (sinoParams->v_d0 + sinoParams->Delta_dv/2))/ sinoParams->Delta_dv + 0.5;
                temp_stop = _MIN_(temp_stop, sinoParams->N_dv-1);

                A->i_vstride[j_x][j_y][i_beta] = _MAX_(temp_stop - A->i_vstart[j_x][j_y][i_beta] + 1, 0);

                /* Auxiliary for using C */
                A->j_u[j_x][j_y][i_beta] = (u_v - (A->u_0+imgParams->Delta_xy/2)) / A->Delta_u + 0.5;

                cosine = cos(alpha_xy);
                
                for (i_v = A->i_vstart[j_x][j_y][i_beta]; i_v < A->i_vstart[j_x][j_y][i_beta]+A->i_vstride[j_x][j_y][i_beta]; ++i_v)
                {
                    /* Calculate B_(i_y, i_beta, j) eq (6) */
                    v_d = (sinoParams->v_d0 + sinoParams->Delta_dv/2) + i_v * sinoParams->Delta_dv;

                    delta_v = v_d - M * v_v;
                    delta_v = _ABS_(delta_v);

                    /* L_v = max{ a - max(|b|, c), 0} */
                    L_v = (W_pv - sinoParams->Delta_dv)/2;           /* b                         */
                    L_v = _ABS_(L_v);                                /* |b|                         */
                    L_v = _MAX_(L_v, delta_v);                        /* max(|b|, c)                 */
                    L_v = (W_pv + sinoParams->Delta_dv)/2 - L_v;    /* a - max(|b|, c)             */
                    L_v = _MAX_(L_v, 0);                            /* max{ a - max(|b|, c), 0} */

                    B_ij = imgParams->Delta_xy * L_v / (cosine * sinoParams->Delta_dv);     /* cosine = cos(alpha_xy) */
                    
                    /* store B_ij in A->B, see (16) (21) for data structure */
                    #if ISBIJCOMPRESSED == 1
                        A->B[j_x][j_y][i_beta*A->i_vstride_max + i_v-A->i_vstart[j_x][j_y][i_beta]] = (B_ij / A->B_ij_scaler) + 0.5;
                    #else
                        A->B[j_x][j_y][i_beta*A->i_vstride_max + i_v-A->i_vstart[j_x][j_y][i_beta]] = B_ij;
                    #endif
                }

            }
        }
    }
}


void computeCMatrix( struct SinoParams *sinoParams, struct ImageParams *imgParams, struct SysMatrix *A)
{
        float u_v, w_v;
        float M;
        float W_pw;
        float w_d;
        float delta_w;
        float L_w;
        float C_ij;

        long int j_u, j_z, i_w;
        int temp_stop;


        for (j_u = 0; j_u <= A->N_u-1; ++j_u)
        {
            /* retrieve voxel center in image coordinates, u coordinate */
            u_v = j_u * A->Delta_u + (A->u_0+imgParams->Delta_xy/2);
            
            /* magnification */
            M = (sinoParams->u_d0 - sinoParams->u_s) / (u_v - sinoParams->u_s);
            
            /* size of flattened voxel footprint on detector */
            W_pw = M * imgParams->Delta_z;

            for (j_z = 0; j_z <= imgParams->N_z-1; ++j_z)
            {
                /* w_v = voxel center in image coordinates, w coordinate (height) */
                w_v = j_z * imgParams->Delta_z + (imgParams->z_0 + imgParams->Delta_z/2);
                
                /* compute start coordinate of voxel footprint in detector index */
                A->i_wstart[j_u][j_z] = (M * w_v - (sinoParams->w_d0 + sinoParams->Delta_dw/2) - W_pw/2 ) * (1/sinoParams->Delta_dw) + 0.5;   /* +0.5 for rounding works because nonnegative */
                A->i_wstart[j_u][j_z] = _MAX_(A->i_wstart[j_u][j_z], 0);

                /* compute end coordinate of voxel footprint in detector index */
                temp_stop = (M * w_v - (sinoParams->w_d0 + sinoParams->Delta_dw/2) + W_pw/2 ) * (1/sinoParams->Delta_dw) + 0.5;    /* +0.5 for rounding works because nonnegative */
                temp_stop = _MIN_(temp_stop, sinoParams->N_dw-1);

                A->i_wstride[j_u][j_z] = _MAX_(temp_stop - A->i_wstart[j_u][j_z] + 1, 0);

                for (i_w = A->i_wstart[j_u][j_z]; i_w < A->i_wstart[j_u][j_z]+A->i_wstride[j_u][j_z]; ++i_w)
                {
                    w_d = (sinoParams->w_d0 + sinoParams->Delta_dw/2) + i_w * sinoParams->Delta_dw;
                    
                    delta_w = w_d - M*w_v;
                    delta_w = _ABS_(delta_w);

                    /* L_w = max{a - max{|b|, c}, 0} */
                    L_w = (W_pw - sinoParams->Delta_dw)/2;            /* b */
                    L_w = _ABS_(L_w);                                /* |b| */
                    L_w = _MAX_(L_w, delta_w);                        /* max{|b|, c} */
                    L_w = (W_pw + sinoParams->Delta_dw)/2 - L_w;    /* a - max{|b|, c} */
                    L_w = _MAX_(L_w, 0);                            /* max{a - max{|b|, c}, 0} */

                    /* alpha_z = 1 / sqrt( 1 + (w_v*w_v)/((u_v-u_s)*(u_v-u_s)) ) */
                    C_ij = (1/sinoParams->Delta_dw)
                            * sqrt( 1 + (w_v*w_v)/((u_v-sinoParams->u_s)*(u_v-sinoParams->u_s)) )
                            * L_w;

                    /* store C_ij in A->C, see (17) (22) (23) for data structure*/
                    #if ISCIJCOMPRESSED == 1
                        A->C[j_u][j_z*A->i_wstride_max + i_w-A->i_wstart[j_u][j_z]] = (C_ij / A->C_ij_scaler) + 0.5;
                    #else
                        A->C[j_u][j_z*A->i_wstride_max + i_w-A->i_wstart[j_u][j_z]] = C_ij;
                    #endif
                }
            }
        }

}


/* write the System matrix to hard drive */
void writeSysMatrix(char *fName, struct SinoParams *sinoParams, struct ImageParams *imgParams, struct SysMatrix *A)
{
    FILE *fp;
    long int totsize = 0;
    long int N_x, N_y, N_z, N_beta, i_vstride_max, i_wstride_max, N_u;
    
    printf("\nWriting System Matrix to %s \n", fName);
    
    fp = fopen(fName, "w");
    if (fp == NULL)
    {
        fprintf(stderr, "ERROR in WriteSysMatrix: can't open file %s.\n", fName);
        exit(-1);
    }
    
    /**
     *      Writing simple variables
     *      i_vstride_max, i_wstride_max, N_u, Delta_u, u_0 and u_1
     *      to file
     */

    totsize += keepWritingToBinaryFile(fp, &(A->i_vstride_max),     1, sizeof(long int), fName);
    totsize += keepWritingToBinaryFile(fp, &(A->i_wstride_max),     1, sizeof(long int), fName);
    totsize += keepWritingToBinaryFile(fp, &(A->N_u),               1, sizeof(long int), fName);
    totsize += keepWritingToBinaryFile(fp, &(A->B_ij_max),          1, sizeof(float), fName);
    totsize += keepWritingToBinaryFile(fp, &(A->C_ij_max),          1, sizeof(float), fName);
    totsize += keepWritingToBinaryFile(fp, &(A->B_ij_scaler),       1, sizeof(float), fName);
    totsize += keepWritingToBinaryFile(fp, &(A->C_ij_scaler),       1, sizeof(float), fName);
    totsize += keepWritingToBinaryFile(fp, &(A->Delta_u),           1, sizeof(float), fName);
    totsize += keepWritingToBinaryFile(fp, &(A->u_0),               1, sizeof(float), fName);
    totsize += keepWritingToBinaryFile(fp, &(A->u_1),               1, sizeof(float), fName);

    /**
     *      Writing array variables
     *      B, i_vstart, i_vstride, j_u, C, i_wstart and i_wstride
     *      to file
     */
    N_x = imgParams->N_x;
    N_y = imgParams->N_y;
    N_z = imgParams->N_z;
    N_beta = sinoParams->N_beta;
    i_vstride_max = A->i_vstride_max;
    i_wstride_max = A->i_wstride_max;
    N_u = A->N_u;

    totsize += keepWritingToBinaryFile(fp, &(A->B[0][0][0]),        N_x*N_y*N_beta*i_vstride_max,   sizeof(BIJDATATYPE), fName);
    totsize += keepWritingToBinaryFile(fp, &(A->i_vstart[0][0][0]), N_x*N_y*N_beta,                 sizeof(INDEXSTARTSTOPDATATYPE),   fName);
    totsize += keepWritingToBinaryFile(fp, &(A->i_vstride[0][0][0]),N_x*N_y*N_beta,                 sizeof(INDEXSTRIDEDATATYPE),   fName);
    totsize += keepWritingToBinaryFile(fp, &(A->j_u[0][0][0]),      N_x*N_y*N_beta,                 sizeof(INDEXJUDATATYPE),   fName);

    totsize += keepWritingToBinaryFile(fp, &(A->C[0][0]),           N_u*N_z*i_wstride_max,          sizeof(CIJDATATYPE), fName);
    totsize += keepWritingToBinaryFile(fp, &(A->i_wstart[0][0]),    N_u*N_z,                        sizeof(INDEXSTARTSTOPDATATYPE),   fName);
    totsize += keepWritingToBinaryFile(fp, &(A->i_wstride[0][0]),   N_u*N_z,                        sizeof(INDEXSTRIDEDATATYPE),   fName);
    
    printf("Total size written = %e GB\n", totsize/1e9);

    fclose(fp);
 
}



/* read the System matrix from hard drive */
/* Utility for reading the Sparse System Matrix */
/* Returns 0 if no error occurs */
void readSysMatrix(char *fName, struct SinoParams *sinoParams, struct ImageParams *imgParams, struct SysMatrix *A)
{

    FILE *fp;
    long int totsize = 0;
    long int N_x, N_y, N_z, N_beta, i_vstride_max, i_wstride_max, N_u;


    fp = fopen(fName, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "ERROR in WriteSysMatrix: can't open file %s.\n", fName);
        exit(-1);
    }
    
    /**
     *      Reading simple variables
     *      i_vstride_max, i_wstride_max, N_u, Delta_u, u_0 and u_1
     *      from file
     */
    
    totsize += keepReadingFromBinaryFile(fp, &(A->i_vstride_max),   1, sizeof(long int), fName);
    totsize += keepReadingFromBinaryFile(fp, &(A->i_wstride_max),   1, sizeof(long int), fName);
    totsize += keepReadingFromBinaryFile(fp, &(A->N_u),             1, sizeof(long int), fName);
    totsize += keepReadingFromBinaryFile(fp, &(A->B_ij_max),        1, sizeof(float), fName);
    totsize += keepReadingFromBinaryFile(fp, &(A->C_ij_max),        1, sizeof(float), fName);
    totsize += keepReadingFromBinaryFile(fp, &(A->B_ij_scaler),     1, sizeof(float), fName);
    totsize += keepReadingFromBinaryFile(fp, &(A->C_ij_scaler),     1, sizeof(float), fName);
    totsize += keepReadingFromBinaryFile(fp, &(A->Delta_u),         1, sizeof(float), fName);
    totsize += keepReadingFromBinaryFile(fp, &(A->u_0),             1, sizeof(float), fName);
    totsize += keepReadingFromBinaryFile(fp, &(A->u_1),             1, sizeof(float), fName);

    /**
     *          Note: Allocation has to happen here (after reading part of the file).
     *          This is because i_vstride_max, i_wstride_max and N_u are unknown before SysMatrix file is read
     *          but they are required to determine the array dimensions
     */
    N_x = imgParams->N_x;
    N_y = imgParams->N_y;
    N_z = imgParams->N_z;
    N_beta = sinoParams->N_beta;
    i_vstride_max = A->i_vstride_max;
    i_wstride_max = A->i_wstride_max;
    N_u = A->N_u;

    allocateSysMatrix(A, N_x, N_y, N_z, N_beta, i_vstride_max, i_wstride_max, N_u);

    /**
     *      Reading array variables
     *      B, i_vstart, i_vstride, j_u, C, i_wstart and i_wstride
     *      from file
     */
    totsize += keepReadingFromBinaryFile(fp, &(A->B[0][0][0]),     N_x*N_y*N_beta*i_vstride_max, sizeof(BIJDATATYPE), fName);
    totsize += keepReadingFromBinaryFile(fp, &(A->i_vstart[0][0][0]), N_x*N_y*N_beta,         sizeof(INDEXSTARTSTOPDATATYPE),   fName);
    totsize += keepReadingFromBinaryFile(fp, &(A->i_vstride[0][0][0]),N_x*N_y*N_beta,         sizeof(INDEXSTRIDEDATATYPE),   fName);
    totsize += keepReadingFromBinaryFile(fp, &(A->j_u[0][0][0]),      N_x*N_y*N_beta,         sizeof(INDEXJUDATATYPE),   fName);

    totsize += keepReadingFromBinaryFile(fp, &(A->C[0][0]),        N_u*N_z*i_wstride_max,        sizeof(CIJDATATYPE), fName);
    totsize += keepReadingFromBinaryFile(fp, &(A->i_wstart[0][0]),    N_u*N_z,                sizeof(INDEXSTARTSTOPDATATYPE),   fName);
    totsize += keepReadingFromBinaryFile(fp, &(A->i_wstride[0][0]),   N_u*N_z,                sizeof(INDEXSTRIDEDATATYPE),   fName);
    
    //printf("Total size read = %e GB\n", totsize/1e9);

    fclose(fp);
    
}



void allocateSysMatrix(struct SysMatrix *A, long int N_x, long int N_y, long int N_z, long int N_beta, long int i_vstride_max, long int i_wstride_max, long int N_u)
{
    /*float totSizeGB;*/

    /*
    totSizeGB =\
    (\
    N_x * N_y * N_beta * i_vstride_max * sizeof(BIJDATATYPE) + \
    N_x * N_y * N_beta * sizeof(INDEXSTARTSTOPDATATYPE) + \
    N_x * N_y * N_beta * sizeof(INDEXSTRIDEDATATYPE) + \
    N_x * N_y * N_beta * sizeof(INDEXJUDATATYPE) + \
    N_u * N_z * i_wstride_max * sizeof(CIJDATATYPE) + \
    N_u * N_z * sizeof(INDEXSTARTSTOPDATATYPE) + \
    N_u * N_z * sizeof(INDEXSTRIDEDATATYPE)\
    )\
    /1e9;*/
   /* printf("\tAllocating %e GB ...\n", totSizeGB);*/


    A->B =          (BIJDATATYPE***)                multialloc(sizeof(BIJDATATYPE), 3, N_x, N_y, N_beta*i_vstride_max);
    A->i_vstart =   (INDEXSTARTSTOPDATATYPE***)     multialloc(sizeof(INDEXSTARTSTOPDATATYPE), 3, N_x, N_y, N_beta);
    A->i_vstride =    (INDEXSTRIDEDATATYPE***)      multialloc(sizeof(INDEXSTRIDEDATATYPE), 3, N_x, N_y, N_beta);
    A->j_u =        (INDEXJUDATATYPE***)            multialloc(sizeof(INDEXJUDATATYPE), 3, N_x, N_y, N_beta);

    A->C =          (CIJDATATYPE**)                multialloc(sizeof(CIJDATATYPE), 2, N_u, N_z*i_wstride_max);
    A->i_wstart =   (INDEXSTARTSTOPDATATYPE**)      multialloc(sizeof(INDEXSTARTSTOPDATATYPE), 2, N_u, N_z);
    A->i_wstride =    (INDEXSTRIDEDATATYPE**)       multialloc(sizeof(INDEXSTRIDEDATATYPE), 2, N_u, N_z);
}

void freeSysMatrix(struct SysMatrix *A)
{
    multifree((void***)A->B, 3);
    multifree((void***)A->i_vstart, 3);
    multifree((void***)A->i_vstride, 3);
    multifree((void***)A->j_u, 3);
    multifree((void**)A->C, 2);
    multifree((void**)A->i_wstart, 2);
    multifree((void**)A->i_wstride, 2);
}


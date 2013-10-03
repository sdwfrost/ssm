/**************************************************************************
 *    This file is part of ssm.
 *
 *    ssm is free software: you can redistribute it and/or modify it
 *    under the terms of the GNU General Public License as published
 *    by the Free Software Foundation, either version 3 of the
 *    License, or (at your option) any later version.
 *
 *    ssm is distributed in the hope that it will be useful, but
 *    WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public
 *    License along with ssm.  If not, see
 *    <http://www.gnu.org/licenses/>.
 *************************************************************************/

#include "ssm.h"



/**
 * Function used by f_prediction_ode_rk:
 * dX/dt = f(t, X, params)
 *
 */
int step_ekf(double t, const double X[], double f[], void *params)
{

    ssm_calc_t *calc = (ssm_calc_t *) params;
    ssm_nav_t *nav = calc->_nav;
    ssm_par_t *par = calc->_par;

    ssm_it_states_t *states_diff = nav->states_diff;
    ssm_it_states_t *states_inc = nav->states_inc;
    ssm_it_states_t *states_sv = nav->states_sv;
    int m = nav->states_sv->length + nav->states_inc->length + nav->states_diff->length;

    gsl_matrix *Ft = calc->Ft;
    gsl_matrix *Q = calc->Q;
    gsl_matrix *FtCt =calc>FtCt;

    gsl_matrix_const_view Ct   = gsl_matrix_const_view_array(&X[m], m, m);
    gsl_matrix_view ff = gsl_matrix_view_array(&f[m], m, m);

    double _r[{{ step.caches|length }}];

    {% if step.sf %}
    double _sf[{{ step.sf|length }}];{% endif %}

    {% if is_diff %}
    int i;
    double diffed[states_diff->length];
    int is_diff = ! (nav->noises_off & SSM_NO_DIFF);
    {% endif %}

    {% if is_diff %}
    for(i=0; i<states_diff->length; i++){
        ssm_parameter_t *p = states_diff->p[i];
        {% if noises_off != 'ode'%}
        if(is_diff){
            diffed[i] = p->f_inv(X[p->offset]);
        } else {
            diffed[i] = gsl_vector_get(par, p->ic->offset);
        }
        {% else %}
        diffed[i] = gsl_vector_get(par, p->ic->offset);
        {% endif %}
    }
    {% endif %}

    /* caches */
    {% for sf in step.sf %}
    _sf[{{ loop.index0 }}] = {{ sf }};{% endfor %}

    {% for cache in step.caches %}
    _r[{{ loop.index0 }}] = {{ cache }};{% endfor %}


    /*ODE system*/

    {% for eq in step.func.ode.proc.system %}
    f[{{eq.index}}] = {{ eq.eq }};{% endfor %}


    //TODO: drift of the diffusion
    //for(i=0; i<states_diff->length; i++){
    //    f[states_diff->p[i]->offset] = 0.0;
    //}


    /*compute incidence:integral between t and t+1*/
    {% for eq in step.func.ode.obs %}
    f[states_inc->p[{{ eq.index }}]->offset] = {{ eq.eq }};{% endfor %}


    ////////////////
    // covariance //
    ////////////////

    // evaluate Q and jacobian
    calc->eval_Q(X, t, par, nav, calc);
    eval_jac(X, t, par, nav, calc);

    // compute Ft*Ct+Ct*Ft'+Q
    //here Ct is symmetrical and transpose(FtCt) == transpose(Ct)transpose(Ft) == Ct transpose(Ft)
    gsl_blas_dgemm (CblasNoTrans, CblasNoTrans, 1.0, Ft, &Ct.matrix, 0.0, FtCt);
    for(i=0; i< ff.matrix.size1; i++){
        for(c=0; c< ff.matrix.size2; c++){
            gsl_matrix_set(&ff.matrix,
                           i,
                           c,
                           gsl_matrix_get(FtCt, i, c) + gsl_matrix_get(FtCt, c, i) + gsl_matrix_get(Q, i, c));

        }
    }
}

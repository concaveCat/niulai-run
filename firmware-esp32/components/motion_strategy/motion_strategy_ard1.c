#include "motion_strategy.h"
#include "servo_zero_leg_math.h"
#include "servo_zero_quad_demo.h"
#include <math.h>
#include <string.h>

#define TICK_MS 20U
#define MAX_ACTIVE 2U
static const uint8_t sem_core[4]={1,0,3,2};
static const uint8_t core_sem[4]={1,0,3,2};

typedef struct {
    qw_motion_init_args_t args;
    qw_ard1_config_t cfg;
    servo_zero_quad_demo_plan_t plan;
    servo_zero_quad_demo_live_state_t live;
    servo_zero_quad_demo_pose_t written,pending;
    servo_zero_quad_demo_live_state_t pending_live;
    uint8_t accepted,observed,requested,contact;
    uint8_t soft_limit_latched;
    bool pending_frame;
    qw_motion_snapshot_t snapshot;
} runtime_t;
static runtime_t s;

static unsigned count4(uint8_t m){ unsigned n=0; for(unsigned i=0;i<4;i++) n+=(m>>i)&1U; return n; }
static uint8_t to_core(uint8_t m){uint8_t r=0;for(uint8_t i=0;i<4;i++)if(m&(1U<<i))r|=1U<<sem_core[i];return r;}
static uint8_t to_semantic(uint8_t m){uint8_t r=0;for(uint8_t i=0;i<4;i++)if(m&(1U<<i))r|=1U<<core_sem[i];return r;}
static const char *name(leg_command_mode_t m){switch(m){case LEG_CMD_LIFT:return "LIFT";case LEG_CMD_SWING:return "SWING";case LEG_CMD_SEARCH:return "SEARCH";default:return "SUPPORT";}}
static qw_motion_state_code_t code(leg_command_mode_t m){switch(m){case LEG_CMD_LIFT:return QW_MOTION_STATE_LIFT;case LEG_CMD_SWING:return QW_MOTION_STATE_SWING;case LEG_CMD_SEARCH:return QW_MOTION_STATE_SEARCH;default:return QW_MOTION_STATE_SUPPORT;}}
static leg_command_mode_t mode(const runtime_t *r,uint8_t l){const servo_zero_quad_demo_live_state_t *v=r->pending_frame?&r->pending_live:&r->live;uint8_t b=1U<<l;if(v->search_leg_mask&b)return LEG_CMD_SEARCH;if(!(v->held_leg_mask&b))return LEG_CMD_HOLD_SUPPORT;return v->lift_elapsed_ms[l]<r->plan.lift_ms?LEG_CMD_LIFT:LEG_CMD_SWING;}
static void transition(runtime_t *r,uint8_t l,leg_command_mode_t a,leg_command_mode_t b,const char *why){if(a==b)return;if(r->snapshot.transition_count==QW_MOTION_TRANSITION_RING_SIZE){memmove(r->snapshot.transitions,r->snapshot.transitions+1,(QW_MOTION_TRANSITION_RING_SIZE-1)*sizeof(r->snapshot.transitions[0]));r->snapshot.transition_count--;}r->snapshot.transitions[r->snapshot.transition_count++]=(qw_motion_transition_t){name(a),name(b),why,0};r->snapshot.transition_total++;(void)l;}

static bool make_plan(runtime_t *r){
 static const uint8_t ch[4]={0,2,4,6};
 static const float hs[4]={836.5f,-836.5f,836.5f,-836.5f},ks[4]={-624.5f,624.5f,-624.5f,624.5f};
 static const uint16_t rest[8]={1494,1165,1672,1748,1368,1279,1764,1934},mn[8]={1400,500,500,500,500,500,1120,500},mx[8]={2500,2500,1750,2500,1940,2500,2500,2500};
 servo_zero_quad_demo_params_t p={.step_length_mm=r->cfg.step_length_mm,.stance_step_length_mm=9,.lift_back_mm=6,.lift_mm=r->cfg.lift_z_mm-r->cfg.touchdown_z_mm,.active_hip_scale_percent=50,.settle_ms=SERVO_ZERO_QUAD_DEMO_DEFAULT_SETTLE_MS,.lift_ms=r->cfg.lift_duration_ms,.swing_ms=r->cfg.swing_duration_ms,.search_ms=r->cfg.search_duration_ms};
 p.pulse_limits.configured=true;
 for(uint8_t l=0;l<4;l++){p.start_foot[l]=(servo_zero_leg_point_t){0,r->cfg.touchdown_z_mm};p.leg[l]=(servo_zero_quad_demo_leg_config_t){.hip_channel=ch[l],.knee_channel=ch[l]+1,.knee_branch=SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD,.pulse_map={rest[ch[l]],rest[ch[l]+1],hs[l],ks[l],300,3000}};}
 for(uint8_t c=0;c<8;c++)p.pulse_limits.channel[c]=(servo_zero_quad_demo_channel_limit_t){true,mn[c],true,mx[c]};
 return servo_zero_quad_demo_make_live_plan(&p,&r->plan);
}
static bool init_live(runtime_t *r){memset(&r->live,0,sizeof r->live);for(uint8_t l=0;l<4;l++){servo_zero_leg_angles_t a;if(!servo_zero_leg_ik(r->plan.start_foot[l],r->plan.leg[l].knee_branch,&a))return false;r->live.support_base_foot[l]=r->plan.start_foot[l];r->live.release_foot[l]=r->plan.start_foot[l];r->live.release_angles[l]=a;r->live.support_safe_rear_x_mm[l]=servo_zero_quad_demo_support_safe_rear_x_mm(&r->plan,l,r->plan.start_foot[l]);if(!isfinite(r->live.support_safe_rear_x_mm[l]))return false;r->live.support_safe_rear_valid_mask|=1U<<l;}r->written=servo_zero_quad_demo_live_pose(&r->plan,&r->live);return r->written.valid;}
static uint8_t read_mask(runtime_t *r,qw_motion_read_fn f){uint8_t m=0;for(uint8_t l=0;l<4;l++)if(f(r->args.capability_ctx,l))m|=1U<<l;return m;}
static bool refresh_baseline(runtime_t *r,uint8_t mask){servo_zero_quad_demo_params_t p={0};p.pulse_limits=r->plan.pulse_limits;p.step_length_mm=r->plan.step_length_mm;p.stance_step_length_mm=r->plan.stance_step_length_mm;p.lift_back_mm=r->plan.lift_back_mm;p.lift_mm=r->plan.lift_mm;p.active_hip_scale_percent=r->plan.active_hip_scale_percent;p.settle_ms=r->plan.settle_ms;p.lift_ms=r->plan.lift_ms;p.swing_ms=r->plan.swing_ms;p.search_ms=r->plan.search_ms;p.active_leg_mask=r->live.moving_leg_mask;for(uint8_t l=0;l<4;l++){p.start_foot[l]=r->plan.start_foot[l];p.leg[l]=r->plan.leg[l];uint8_t b=1U<<l;if(mask&b){p.start_foot[l]=r->written.foot[l];p.leg[l].pulse_map.hip_rest_us=r->written.pulse_us[p.leg[l].hip_channel];p.leg[l].pulse_map.knee_rest_us=r->written.pulse_us[p.leg[l].knee_channel];}}servo_zero_quad_demo_plan_t next;if(!servo_zero_quad_demo_make_live_plan(&p,&next))return false;r->plan=next;return true;}

static void inputs(runtime_t *r,uint8_t req){uint8_t old=r->accepted,rel=old&~req,rise=req&~r->observed,cr=to_core(rel),nr=to_core(rise);r->observed=req;for(uint8_t l=0;l<4;l++){uint8_t b=1U<<l;if(!(cr&b))continue;r->live.release_foot[l]=r->written.foot[l];r->live.release_angles[l]=r->written.angles[l];r->live.swing_offset_mm[l]=r->written.accepted_swing_offset_mm[l];r->live.search_elapsed_ms[l]=0;r->live.held_leg_mask&=~b;r->live.search_leg_mask|=b;r->soft_limit_latched&=~b;transition(r,sem_core[l],mode(r,l),LEG_CMD_SEARCH,"release");}if(cr&&r->written.body_drive_mm>r->live.body_drive_floor_mm)r->live.body_drive_floor_mm=r->written.body_drive_mm;r->accepted&=req;if(!rise|| (nr&r->live.moving_leg_mask)||count4(r->live.moving_leg_mask|nr)>MAX_ACTIVE)return;/* A held leg may already be in SWING when another press arrives. The two-leg budget and geometry/limit checks remain the admission gate. */if(!refresh_baseline(r,nr))return;r->accepted|=rise;r->live.moving_leg_mask|=nr;r->live.held_leg_mask|=nr;for(uint8_t l=0;l<4;l++)if(nr&(1U<<l)){r->live.swing_offset_mm[l]=0;r->live.lift_elapsed_ms[l]=r->live.swing_elapsed_ms[l]=r->live.search_elapsed_ms[l]=0;transition(r,sem_core[l],LEG_CMD_HOLD_SUPPORT,LEG_CMD_LIFT,"press");}}
static float progress(const runtime_t *r,uint8_t l,leg_command_mode_t m){const servo_zero_quad_demo_live_state_t *s0=r->pending_frame?&r->pending_live:&r->live;uint32_t v=0,n=1;if(m==LEG_CMD_LIFT){v=s0->lift_elapsed_ms[l];n=r->plan.lift_ms;}else if(m==LEG_CMD_SWING){v=s0->swing_elapsed_ms[l];n=r->plan.swing_ms;}else if(m==LEG_CMD_SEARCH){v=s0->search_elapsed_ms[l];n=r->plan.search_ms;}return n?(float)v/(float)n:0;}
static void emit_pose(runtime_t *r){qw_motion_frame_t f={.command_mask=15,.joint_target_mask=15};for(uint8_t sem=0;sem<4;sem++){uint8_t l=sem_core[sem];leg_command_mode_t m=mode(r,l);f.legs[sem]=(leg_command_t){m,fminf(1,progress(r,l,m))};f.joints[sem]=(qw_motion_joint_target_t){r->pending.angles[l].hip_rad,r->pending.angles[l].knee_rad,r->pending.valid};}r->args.emit(&f,r->args.capability_ctx);}

static bool init(void *ctx,const qw_motion_init_args_t *a){runtime_t *r=ctx;if(!r||!a||!a->read_pressed||!a->read_contact||!a->emit||(a->leg_mask&15)!=15)return false;memset(r,0,sizeof*r);r->args=*a;r->cfg=a->strategy_config?*(const qw_ard1_config_t*)a->strategy_config:(qw_ard1_config_t){300,360,120,30,-60,-100};if(!r->cfg.lift_duration_ms||!r->cfg.swing_duration_ms||!r->cfg.search_duration_ms||!isfinite(r->cfg.step_length_mm)||r->cfg.step_length_mm<=0||!isfinite(r->cfg.lift_z_mm)||!isfinite(r->cfg.touchdown_z_mm)||!make_plan(r)||!init_live(r))return false;r->snapshot=(qw_motion_snapshot_t){.strategy_name="ard1_shared_live_core",.state_name="SUPPORT",.state=QW_MOTION_STATE_SUPPORT};return true;}
static uint32_t advance(uint32_t v,uint32_t max){if(v>=max)return max;return v+(max-v<TICK_MS?max-v:TICK_MS);}
static void advance_live(runtime_t *r, servo_zero_quad_demo_live_state_t *l)
{ for(uint8_t n=0;n<4;n++){uint8_t b=1U<<n;if(l->held_leg_mask&b){if(l->lift_elapsed_ms[n]<r->plan.lift_ms)l->lift_elapsed_ms[n]=advance(l->lift_elapsed_ms[n],r->plan.lift_ms);else if(!(r->soft_limit_latched&b))l->swing_elapsed_ms[n]=advance(l->swing_elapsed_ms[n],r->plan.swing_ms);if(l->lift_elapsed_ms[n]>=r->plan.lift_ms&&! (r->soft_limit_latched&b))l->swing_offset_mm[n]=fminf((float)l->swing_elapsed_ms[n]*r->plan.step_length_mm/r->plan.swing_ms,r->plan.leg_step_length_mm[n]);}if(l->search_leg_mask&b)l->search_elapsed_ms[n]=advance(l->search_elapsed_ms[n],r->plan.search_ms);}} 
static void tick(void *ctx,float dt){runtime_t *r=ctx;if(!r||r->pending_frame)return;(void)dt;r->requested=read_mask(r,r->args.read_pressed);inputs(r,r->requested);r->contact=to_core(read_mask(r,r->args.read_contact));r->pending_live=r->live;advance_live(r,&r->pending_live);r->pending=servo_zero_quad_demo_live_pose(&r->plan,&r->pending_live);r->pending_frame=true;emit_pose(r);}
static void reset(void *ctx){runtime_t *r=ctx;if(r){qw_motion_init_args_t a=r->args;init(r,&a);}}
static void commit_shared(void *ctx,bool applied);
static bool snapshot(void *ctx,qw_motion_snapshot_t *out);
bool qw_motion_ard1_create(qw_motion_strategy_t *strategy){if(!strategy)return false;memset(&s,0,sizeof s);*strategy=(qw_motion_strategy_t){&s,init,tick,commit_shared,snapshot,reset};return true;}
static void commit_shared(void *ctx,bool applied){runtime_t *r=ctx;if(!r||!r->pending_frame)return;if(!applied||!r->pending.valid){r->pending_frame=false;return;}r->written=r->pending;r->soft_limit_latched|=r->pending.soft_limit_leg_mask&r->live.held_leg_mask;r->live=r->pending_live;for(uint8_t q=0;q<4;q++)if(r->pending.soft_limit_leg_mask&(1U<<q))r->live.swing_offset_mm[q]=r->pending.accepted_swing_offset_mm[q];uint8_t landed=0;for(uint8_t l=0;l<4;l++)if((r->live.search_leg_mask&(1U<<l))&&r->live.search_elapsed_ms[l]>=r->plan.search_ms)landed|=1U<<l;if(landed){r->live.moving_leg_mask&=~landed;r->live.search_leg_mask&=~landed;if(!servo_zero_quad_demo_rebase_written_pose(&r->plan,&r->live,&r->written,landed))r->snapshot.fault=true;for(uint8_t l=0;l<4;l++)if(landed&(1U<<l))transition(r,sem_core[l],LEG_CMD_SEARCH,LEG_CMD_HOLD_SUPPORT,"touchdown");}r->pending_frame=false;}
static bool snapshot(void *ctx,qw_motion_snapshot_t *out){runtime_t *r=ctx;if(!r||!out)return false;*out=r->snapshot;out->stepping_leg_mask=to_semantic(r->live.moving_leg_mask);if(!out->fault){leg_command_mode_t m=r->live.search_leg_mask?LEG_CMD_SEARCH:r->live.held_leg_mask?LEG_CMD_SWING:LEG_CMD_HOLD_SUPPORT;if(m==LEG_CMD_SWING)for(uint8_t l=0;l<4;l++)if((r->live.held_leg_mask&(1U<<l))&&r->live.lift_elapsed_ms[l]<r->plan.lift_ms)m=LEG_CMD_LIFT;out->state=code(m);out->state_name=name(m);}return true;}

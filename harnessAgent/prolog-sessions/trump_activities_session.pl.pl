sleep(A) :-
   wasm_generic:host_rpc(sleep(A)).
delay(A) :-
   wasm_generic:host_rpc(delay(A)).
console_log(A) :-
   wasm_generic:host_rpc(console_log(A)).
js_eval(A,B) :-
   wasm_generic:host_rpc(js_eval(A,B)).
js_eval_json(A,B) :-
   wasm_generic:host_rpc(js_eval_json(A,B)).
future(A,B) :-
   wasm_generic:host_rpc(future(A,B)).
await_any(A,B,C) :-
   wasm_generic:host_rpc(await_any(A,B,C)).
future_cancel(A) :-
   wasm_generic:host_rpc(future_cancel(A)).
action(trump,sign_executive_order,boost_nuclear_energy).
action(trump,call_with_putin,peace_in_ukraine).
action(trump,crackdown,harvard).
action(trump,gala,crypto).
action(trump,overturn_executive_order,law_firm_jenner_block).
action(trump,response_to_murders,israeli_embassy_staffers).
action(trump,push,autism_study_deadline).
action(trump,call_on_farmers,support_maha_agenda).
action(trump,call_with_putin,ukraine_peace_talks).
action(trump,threaten_tariffs,eu_goods).
action(trump,halt_plan,harvard_foreign_students).
action(trump,push,golden_dome_project).
action(trump,make_exception,white_south_african_refugees).
action(trump,confront,ramaphosa).
action_category(A,executive_order) :-
   action(trump,sign_executive_order,A).
action_category(A,political_move) :-
   action(trump,call_with_putin,A).
action_category(A,political_move) :-
   action(trump,crackdown,A).
action_category(A,political_move) :-
   action(trump,gala,A).
action_category(A,legal_action) :-
   action(trump,overturn_executive_order,A).
action_category(A,foreign_policy) :-
   action(trump,response_to_murders,A).
action_category(A,domestic_policy) :-
   action(trump,push,A).
action_category(A,domestic_policy) :-
   action(trump,call_on_farmers,A).
action_category(A,foreign_policy) :-
   action(trump,call_with_putin,A).
action_category(A,foreign_policy) :-
   action(trump,threaten_tariffs,A).
action_category(A,domestic_policy) :-
   action(trump,halt_plan,A).
action_category(A,domestic_policy) :-
   action(trump,push,A).
action_category(A,legal_action) :-
   action(trump,make_exception,A).
action_category(A,political_move) :-
   action(trump,confront,A).
busy_day(trump) :-
   findall(A,action(trump,B,A),C),length(C,D),D>=3.
controversial_action(A) :-
   action(trump,confront,A).
controversial_action(A) :-
   action(trump,threaten_tariffs,A).
high_impact_action(A) :-
   action_category(A,foreign_policy).
high_impact_action(A) :-
   action_category(A,domestic_policy).
diplomatic_engagement(A) :-
   action(trump,call_with_putin,A).
legal_exception(A) :-
   action(trump,make_exception,A).
legal_exception(A) :-
   action(trump,overturn_executive_order,A).
focus_area(A) :-
   findall(B,action_category(B,A),C),length(C,D),D>=2.
today_activity('Donald Trump','gave a speech at West Point').
today_activity('Donald Trump','threatened tariffs on EU and smartphones').
is_politician('Donald Trump').
is_active('Donald Trump').
attended_event('Donald Trump','West Point speech',today).
made_statement('Donald Trump','threatened tariffs on EU and smartphones',today).
signed_executive_order('Donald Trump','boost nuclear energy',today).
faced_criticism('Donald Trump','politicizing justice',today).
engaged_in_discussion('Donald Trump','Ukraine peace talks',today).
is_controversial('Donald Trump').
is_influential('Donald Trump').


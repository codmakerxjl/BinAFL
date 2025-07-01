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
function(sub_1400AB1E0).
variable(sub_1400AB1E0,a1,pointer).
variable(sub_1400AB1E0,a2,int64).
variable(sub_1400AB1E0,a3,int64).
variable(sub_1400AB1E0,a4,int64).
variable(sub_1400AB1E0,v8,pointer).
variable(sub_1400AB1E0,v9,int64).
variable(sub_1400AB1E0,v11,int64).
variable(sub_1400AB1E0,v12,int32).
variable(sub_1400AB1E0,v13,int32).
variable(sub_1400AB1E0,v14,int32).
variable(sub_1400AB1E0,v15,int32).
behavior(sub_1400AB1E0,'Checks if v9 equals the value at *(v8 - 8)').
behavior(sub_1400AB1E0,'Calls a function via v9 + 56 if the check passes').
behavior(sub_1400AB1E0,'Returns 0 if v11 is 0').
behavior(sub_1400AB1E0,'Checks conditions involving v14, v13, and v12').
behavior(sub_1400AB1E0,'Returns v11 if conditions are met').
edge_case(sub_1400AB1E0,'Handles negative a4 values').
edge_case(sub_1400AB1E0,'Checks if a1 equals v11 + a4').
function_signature(sub_1400AB1E0,[pObject,offsetOrData1,offsetOrData2,flagOrValue]).
variable_type(pObject,struct_object_ptr).
variable_type(offsetOrData1,int64_t).
variable_type(offsetOrData2,int64_t).
variable_type(flagOrValue,int64_t).
variable_type(pValidationData,char_ptr).
variable_type(objectKey,uint64_t).
variable_type(result,int64_t).
variable_type(bitCheck1,uint32_t).
variable_type(bitCheck2,uint32_t).
variable_type(bitCheck3,uint32_t).
variable_type(unusedVar,int).
derived_from(pValidationData,pObject,'pObject + *(_QWORD *)(*pObject - 16LL)').
derived_from(objectKey,pObject,'*(_QWORD *)(*pObject - 8LL)').
returns(sub_1400AB1E0,result).
edge_case(sub_1400AB1E0,flagOrValue,-2).
edge_case(sub_1400AB1E0,pObject,'result + flagOrValue').
bitwise_condition(sub_1400AB1E0,bitCheck1,bitCheck2,bitCheck3,'(v13 & v12 & 6) != 6').
bitwise_condition(sub_1400AB1E0,bitCheck2,'5','4','(v13 & 5) != 4').
bitwise_condition(sub_1400AB1E0,bitCheck3,'6','6','(v14 & 6) != 6').
calls(sub_1400AB1E0,'[rax+38h]',[pObject,offsetOrData1,offsetOrData2,flagOrValue]).
calls(sub_1400AB1E0,'[r10+40h]',[pObject,offsetOrData1,offsetOrData2,flagOrValue]).


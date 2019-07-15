__kernel void
vector_add_debug_trap(__global int *a,
                      __global int *b,
                      __global int *c)
{
  local int lds_check;
  lds_check = 1;
  int gid = get_global_id(0);
  c[gid] = a[gid] + b[gid];
  __builtin_trap();
}

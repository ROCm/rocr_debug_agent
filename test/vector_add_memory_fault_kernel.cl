__kernel void
vector_add_memory_fault(__global int *a,
                        __global int *b,
                        __global int *c)
{
  local int lds_check;
  lds_check = 1;
  int two = 2;
  int gid = get_global_id(0);
  c[gid*10] = a[gid] + b[gid];
}

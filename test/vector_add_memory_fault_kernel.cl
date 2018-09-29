__kernel void
vector_add_memory_fault(__global int *a,
                        __global int *b,
                        __global int *c)
{
  int gid = get_global_id(0);
  c[gid*10] = a[gid] + b[gid];
}

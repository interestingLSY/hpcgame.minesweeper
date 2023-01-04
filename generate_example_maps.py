#encoding: utf-8
import shutil, os

# Check whether "map_generator" is present
if not os.path.exists('map_generator'):
	print('Error: `map_generator` does not exists. Maybe you should run `make` first?')

# Create the directory
if os.path.exists('map'):
	shutil.rmtree('map', ignore_errors=True)
os.mkdir('map')

sizes = [2**N for N in range(4, 16+1)]
seed = 0

for N in sizes:
	K = N*N//8
	print(f"Generating map with N={N}, K={K}, seed={seed}")
	filename = "%d_%d_%d.map" % (N, K, seed)
	exitcode = os.system(f"./map_generator {N} {K} {seed} > map/{filename}")
	if exitcode != 0:
		print(f"The generator terminated with a non-zero exitcode: {exitcode}")
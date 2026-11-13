"""The set lists, written by scripts/gen_games.py from games.toml: edit that, not this."""

# Model 2 sets, in games.toml's order (recompile.py --set, seed_scan.py --set)
M2_SETS = ('daytona93', 'daytona', 'srallyc', 'manxttc', 'vcop2', 'vcop', 'vf2', 'indy500', 'fvipers', 'sgt24h', 'overrevb', 'stcc', 'von', 'hotdo', 'gunblade', 'vstriker', 'zerogun', 'pltkids', 'motoraid', 'dynamcop', 'skytargt', 'desert', 'lastbrnx', 'schamp', 'dynabb97', 'waverunr', 'segawski', 'skisuprg', 'bel', 'topskatr', 'rchase2', 'doab')
# by board: the original Model 2, 2A-CRX, 2B-CRX (a SHARC for the TGP), 2C-CRX (a TGPx4)
MODEL2O_SETS = ('daytona93', 'daytona', 'vcop', 'desert')
MODEL2A_ONLY_SETS = ('srallyc', 'manxttc', 'vcop2', 'vf2', 'motoraid', 'dynamcop', 'skytargt')
MODEL2B_SETS = ('indy500', 'fvipers', 'sgt24h', 'overrevb', 'von', 'gunblade', 'vstriker', 'zerogun', 'pltkids', 'lastbrnx', 'schamp', 'dynabb97', 'rchase2', 'doab')
MODEL2C_SETS = ('stcc', 'hotdo', 'waverunr', 'segawski', 'skisuprg', 'bel', 'topskatr')
# Model 1 sets (cmake/model1.cmake): their own pipeline
MODEL1_SETS = ('vr', 'vformula', 'swa', 'vf', 'wingwar')
# Other names a set's archive may have (MAME's parent holding a clone's whole set)
OTHER_NAMES = {'hotdo': ('hotd',), 'doab': ('doa',), 'vformula': ('vr',)}

extern uae_u32 bmap_lget(uaecptr addr);
extern uae_u32 bmap_wget(uaecptr addr);
extern uae_u32 bmap_bget(uaecptr addr);

extern void bmap_lput(uaecptr addr, uae_u32 l);
extern void bmap_wput(uaecptr addr, uae_u32 w);
extern void bmap_bput(uaecptr addr, uae_u32 b);

extern void bmap_init(void);

extern int bmap_tpe_select;
extern int bmap_hreq_enable;
extern int bmap_txd_enable;

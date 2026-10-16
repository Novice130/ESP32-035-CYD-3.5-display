"""
creatures.py - fan-tribute pixel art for the Kalos battle sprites.

All coordinates are in sprite pixels on a 64x64 canvas (front sprites face
down-left toward the player, back sprites show the partner from behind).
"""
from artlib import Canvas

BLACK = (34, 30, 40)
WHITE = (255, 255, 255)
EYE = (36, 32, 44)


# ============================================================== PIKACHU
def pikachu_front():
    c = Canvas(64, 64)
    c.part("tailb", (176, 112, 40))
    c.part("tail", (250, 214, 40))
    c.part("body", (250, 214, 40))
    c.part("ear", (250, 214, 40))
    c.part("tip", (48, 40, 40))
    c.part("head", (250, 214, 40))
    c.part("foot", (250, 214, 40))
    c.part("arm", (250, 214, 40))
    c.part("cheek", (236, 64, 48), line=False)
    # lightning tail behind body
    c.poly("tailb", [(40, 50), (46, 44), (49, 47), (43, 53)])
    c.poly("tail", [(44, 46), (52, 38), (48, 35), (56, 26), (52, 23), (62, 8),
                    (63, 10), (60, 24), (64, 27), (56, 36), (59, 39), (49, 49)])
    c.ellipse("body", 31, 47, 13, 12)
    # ears
    c.poly("ear", [(17, 22), (5, 1), (9, 0), (25, 17)])
    c.poly("ear", [(37, 17), (52, 0), (56, 2), (45, 22)])
    c.poly("tip", [(10.6, 9.3), (5, 1), (9, 0), (13.8, 7)])
    c.poly("tip", [(47.2, 6.2), (52, 0), (56, 2), (50.6, 8.7)])
    c.ellipse("head", 31, 28, 16, 13.5)
    c.ellipse("foot", 22, 58, 5.5, 3)
    c.ellipse("foot", 40, 58, 5.5, 3)
    c.ellipse("arm", 23, 45, 4.5, 3.5)
    c.ellipse("arm", 39, 45, 4.5, 3.5)
    c.circle("cheek", 19, 33, 3.6)
    c.circle("cheek", 43, 33, 3.6)
    # face details
    c.eye(23, 25, 3, 4)
    c.eye(37, 25, 3, 4)
    c.px(31, 30, EYE)
    c.pxs([(28, 33), (29, 34), (30, 34), (31, 33), (32, 34), (33, 34), (34, 33)], (120, 60, 40))
    # brown back stripes peek on body side
    return c.render()


def pikachu_back():
    c = Canvas(64, 64)
    c.part("tailb", (176, 112, 40))
    c.part("tail", (250, 214, 40))
    c.part("body", (250, 214, 40))
    c.part("stripe", (176, 112, 40), line=False)
    c.part("ear", (250, 214, 40))
    c.part("tip", (48, 40, 40))
    c.part("head", (250, 214, 40))
    c.ellipse("body", 32, 52, 17, 14)
    c.rect("stripe", 20, 47, 44, 50)
    c.rect("stripe", 19, 54, 45, 57)
    c.poly("tailb", [(42, 54), (47, 49), (50, 52), (45, 58)])
    c.poly("tail", [(46, 51), (54, 42), (50, 39), (58, 30), (54, 27), (64, 12),
                    (64, 14), (62, 28), (64, 31), (58, 40), (61, 43), (51, 54)])
    c.poly("ear", [(19, 26), (3, 6), (7, 4), (28, 20)])
    c.poly("ear", [(36, 20), (54, 4), (58, 7), (45, 26)])
    c.poly("tip", [(9.4, 14.0), (3, 6), (7, 4), (12.3, 10.6)])
    c.poly("tip", [(50.2, 7.7), (54, 4), (58, 7), (52.2, 12.2)])
    c.ellipse("head", 32, 33, 18, 14)
    return c.render()


# ============================================================== FENNEKIN
FEN_FUR = (250, 224, 140)
FEN_CREAM = (255, 244, 214)
FEN_ORANGE = (240, 104, 48)


def fennekin_front():
    c = Canvas(64, 64)
    c.part("tail", FEN_FUR)
    c.part("tailtip", FEN_ORANGE, line=False)
    c.part("legb", FEN_FUR)
    c.part("body", FEN_FUR)
    c.part("ear", FEN_FUR)
    c.part("inner", FEN_ORANGE, line=False, outline=(170, 60, 30))
    c.part("head", FEN_FUR)
    c.part("face", FEN_CREAM, line=False)
    c.part("legf", FEN_FUR)
    # big fluffy tail curling up behind the body
    c.ellipse("tail", 50, 46, 7, 9)
    c.ellipse("tail", 55, 37, 5.5, 6.5)
    c.ellipse("tailtip", 57, 33, 3.5, 3.5)
    c.ellipse("legb", 43, 57, 3, 5)
    c.ellipse("body", 36, 49, 12, 8.5)
    # tall triangular ears with orange fur tufts
    c.poly("ear", [(10, 28), (1, 0), (26, 18)])
    c.poly("ear", [(35, 17), (61, 1), (50, 28)])
    c.poly("inner", [(13, 22), (6, 6), (21, 17)])
    c.poly("inner", [(40, 17), (55, 6), (47, 22)])
    c.ellipse("head", 29, 30, 15, 11.5)
    # cheek fur tufts
    c.poly("head", [(16, 31), (9, 36), (18, 37)])
    c.poly("head", [(42, 31), (49, 36), (40, 37)])
    c.ellipse("face", 28, 36, 10, 5)
    c.ellipse("legf", 27, 57, 3.2, 5.5)
    c.ellipse("legf", 34, 58, 3.2, 5)
    c.ellipse("face", 33, 47, 6, 3.5)
    # big amber eyes
    for ex in (20, 33):
        c.pxs([(ex + i, ey) for i in range(4) for ey in (27, 28, 29)], (214, 84, 34))
        c.pxs([(ex + 1, 30), (ex + 2, 30)], (214, 84, 34))
        c.pxs([(ex + 1, 26), (ex + 2, 26)], EYE)
        c.pxs([(ex + 2, 28), (ex + 2, 29), (ex + 1, 28), (ex + 1, 29)], EYE)
        c.px(ex + 1, 27, WHITE)
    c.pxs([(28, 34), (29, 34)], EYE)
    c.pxs([(26, 37), (27, 38), (28, 38), (29, 38), (30, 37)], (160, 96, 70))
    return c.render()


def fennekin_back():
    c = Canvas(64, 64)
    c.part("tail", FEN_FUR)
    c.part("tailtip", FEN_ORANGE, line=False)
    c.part("body", FEN_FUR)
    c.part("ear", FEN_FUR)
    c.part("inner", (226, 150, 90), line=False)
    c.part("head", FEN_FUR)
    c.ellipse("body", 32, 56, 16, 11)
    c.ellipse("tail", 50, 46, 8, 12)
    c.ellipse("tail", 54, 34, 6, 7)
    c.ellipse("tailtip", 56, 29, 4, 4)
    c.poly("ear", [(14, 36), (2, 4), (28, 26)])
    c.poly("ear", [(36, 26), (60, 2), (50, 36)])
    c.poly("inner", [(10, 22), (5, 8), (14, 20)])
    c.poly("inner", [(54, 12), (57, 7), (52, 18)])
    c.ellipse("head", 32, 38, 16, 13)
    c.poly("head", [(17, 40), (10, 46), (20, 47)])
    c.poly("head", [(47, 40), (54, 46), (44, 47)])
    return c.render()


# ============================================================== FROAKIE
FRO_BLUE = (104, 186, 240)
FRO_DARK = (62, 128, 200)
FRO_FOAM = (240, 250, 255)
FRO_EYE = (255, 232, 110)


def froakie_front():
    c = Canvas(64, 64)
    c.part("legb", FRO_BLUE)
    c.part("body", FRO_BLUE)
    c.part("belly", (190, 228, 250), line=False)
    c.part("head", FRO_BLUE)
    c.part("cap", FRO_DARK, line=False)
    c.part("eye", FRO_EYE, outline=(150, 110, 40))
    c.part("foam", FRO_FOAM, outline=(130, 170, 210))
    c.part("foam2", FRO_FOAM, outline=(130, 170, 210))
    c.part("arm", FRO_BLUE)
    # crouched hind legs
    c.ellipse("legb", 15, 52, 9, 7)
    c.ellipse("legb", 49, 52, 9, 7)
    c.ellipse("legb", 11, 59, 6, 3)
    c.ellipse("legb", 53, 59, 6, 3)
    c.ellipse("body", 32, 48, 13, 11)
    c.ellipse("belly", 32, 51, 8, 7)
    c.ellipse("head", 32, 30, 18, 10)
    c.ellipse("cap", 32, 25, 14, 4)
    # bulging eyes on top of the head
    c.circle("eye", 21, 19, 6.5)
    c.circle("eye", 43, 19, 6.5)
    # frubbles - foamy bubbles around the neck
    for i, (fx, fy, fr) in enumerate(((10, 37, 5), (55, 37, 5), (17, 41, 5.5), (48, 41, 5.5),
                                      (25, 43, 4.5), (40, 43, 4.5), (32, 44, 4))):
        c.circle("foam" if i % 2 else "foam2", fx, fy, fr)
    c.ellipse("arm", 25, 56, 3.2, 5)
    c.ellipse("arm", 39, 56, 3.2, 5)
    c.ellipse("arm", 24, 60, 4, 2)
    c.ellipse("arm", 40, 60, 4, 2)
    # pupils + catch lights
    for ex in (19, 41):
        c.pxs([(ex + i, ey) for i in range(4) for ey in (17, 18, 19, 20, 21)], EYE)
        c.pxs([(ex, 17), (ex + 1, 17)], WHITE)
        c.px(ex, 18, WHITE)
    c.pxs([(28, 34), (29, 35), (30, 35), (31, 35), (32, 35), (33, 35), (34, 35), (35, 34)], (40, 70, 120))
    return c.render()


def froakie_back():
    c = Canvas(64, 64)
    c.part("legb", FRO_BLUE)
    c.part("body", FRO_BLUE)
    c.part("head", FRO_BLUE)
    c.part("cap", FRO_DARK, line=False)
    c.part("eye", FRO_EYE, outline=(150, 110, 40))
    c.part("foam", FRO_FOAM, outline=(130, 170, 210))
    c.part("foam2", FRO_FOAM, outline=(130, 170, 210))
    c.ellipse("legb", 12, 56, 10, 8)
    c.ellipse("legb", 52, 56, 10, 8)
    c.ellipse("body", 32, 54, 16, 12)
    c.ellipse("head", 32, 36, 20, 12)
    c.ellipse("cap", 32, 34, 17, 9)
    c.circle("eye", 19, 24, 7)
    c.circle("eye", 45, 24, 7)
    for (fx, fy, fr) in ((10, 44, 5), (17, 48, 5.5), (25, 50, 5), (32, 51, 5), (39, 50, 5), (47, 48, 5.5),
                         (54, 44, 5), (21, 55, 4), (32, 57, 4.5), (43, 55, 4), (6, 39, 3.5), (58, 39, 3.5)):
        c.circle("foam" if (fx // 7) % 2 else "foam2", fx, fy, fr)
    return c.render()


# ============================================================== CHESPIN
CHE_GREEN = (98, 176, 70)
CHE_TAN = (240, 212, 156)
CHE_BROWN = (150, 98, 58)
CHE_SPIKE = (232, 196, 120)


def chespin_front():
    c = Canvas(64, 64)
    c.part("tail", CHE_BROWN)
    c.part("legs", CHE_BROWN)
    c.part("body", CHE_TAN)
    c.part("spike", CHE_SPIKE)
    c.part("hood", CHE_GREEN)
    c.part("hoodline", (66, 130, 46), line=False, shade=False)
    c.part("face", CHE_TAN)
    c.part("arm", CHE_BROWN)
    c.part("nose", (200, 80, 56), line=False)
    c.ellipse("tail", 48, 52, 6, 5)
    c.ellipse("legs", 25, 57, 6, 5)
    c.ellipse("legs", 39, 57, 6, 5)
    c.ellipse("body", 32, 48, 12, 10)
    # three tan spikes rising from the hood
    c.poly("spike", [(22, 14), (17, 0), (28, 10)])
    c.poly("spike", [(28, 10), (33, -1), (37, 10)])
    c.poly("spike", [(37, 11), (47, 1), (43, 15)])
    c.ellipse("hood", 32, 25, 18, 15)
    c.poly("hood", [(14, 28), (8, 40), (20, 34)])
    c.poly("hood", [(50, 28), (56, 40), (44, 34)])
    c.thick("hoodline", [(17, 24), (32, 18), (47, 24)], 1.2)
    c.ellipse("face", 32, 33, 12, 9)
    c.ellipse("arm", 20, 46, 4, 4.5)
    c.ellipse("arm", 44, 46, 4, 4.5)
    c.poly("nose", [(29, 34), (35, 34), (32, 38)])
    for ex in (24, 36):
        c.pxs([(ex + i, ey) for i in range(3) for ey in (29, 30, 31, 32)], EYE)
        c.px(ex, 29, WHITE)
        c.px(ex + 1, 30, (90, 70, 60))
    c.pxs([(29, 40), (30, 41), (31, 41), (32, 41), (33, 41), (34, 41), (35, 40)], (150, 90, 60))
    return c.render()


def chespin_back():
    c = Canvas(64, 64)
    c.part("tail", CHE_BROWN)
    c.part("body", CHE_BROWN)
    c.part("spike", CHE_SPIKE)
    c.part("hood", CHE_GREEN)
    c.part("hoodline", (66, 130, 46), line=False, shade=False)
    c.ellipse("body", 32, 56, 17, 10)
    c.ellipse("tail", 32, 60, 6, 4)
    c.poly("spike", [(20, 20), (14, 3), (28, 14)])
    c.poly("spike", [(28, 14), (33, 1), (38, 14)])
    c.poly("spike", [(38, 14), (52, 3), (45, 20)])
    c.ellipse("hood", 32, 36, 21, 18)
    c.thick("hoodline", [(16, 34), (32, 26), (48, 34)], 1.3)
    c.thick("hoodline", [(14, 44), (32, 38), (50, 44)], 1.3)
    c.thick("hoodline", [(32, 26), (32, 52)], 1.2)
    c.ellipse("tail", 32, 61, 5, 3)
    return c.render()


# ============================================================== FLETCHLING
def fletchling_front():
    c = Canvas(64, 64)
    ORANGE = (236, 96, 54)
    GREY = (132, 128, 140)
    c.part("tail", (64, 60, 70))
    c.part("legs", (240, 190, 60))
    c.part("body", GREY)
    c.part("chest", (244, 150, 96), line=False)
    c.part("wing", (104, 100, 116))
    c.part("wingtip", (54, 50, 62), line=False)
    c.part("head", ORANGE)
    c.part("mask", (250, 230, 200), line=False)
    c.part("beak", (54, 50, 58))
    c.poly("tail", [(42, 50), (60, 54), (58, 60), (40, 56)])
    c.thick("legs", [(28, 54), (27, 60)], 1.2)
    c.thick("legs", [(35, 54), (36, 60)], 1.2)
    c.thick("legs", [(24, 60), (30, 60)], 1)
    c.thick("legs", [(34, 60), (40, 60)], 1)
    c.ellipse("body", 32, 44, 13, 11)
    c.ellipse("chest", 29, 46, 8, 7)
    # wing raised in a little flap
    c.poly("wing", [(34, 38), (44, 34), (54, 40), (56, 50), (44, 52), (36, 48)])
    c.poly("wingtip", [(48, 46), (56, 50), (50, 53), (44, 52)])
    c.thick("wingtip", [(40, 42), (50, 45)], 1)
    c.ellipse("head", 28, 29, 12, 10)
    c.poly("head", [(30, 19), (36, 14), (36, 22)])
    c.ellipse("mask", 25, 31, 6, 4)
    c.poly("beak", [(16, 30), (11, 32), (17, 33)])
    c.pxs([(22, 27), (23, 27), (22, 28), (23, 28), (24, 28), (23, 29)], EYE)
    c.px(22, 27, WHITE)
    c.pxs([(31, 27), (32, 27), (31, 28), (32, 28), (32, 29)], EYE)
    c.px(31, 27, WHITE)
    return c.render()


# ============================================================== BUNNELBY
def bunnelby_front():
    c = Canvas(64, 64)
    FUR = (166, 144, 124)
    DARK = (106, 84, 72)
    CREAM = (238, 224, 200)
    c.part("feet", DARK)
    c.part("body", FUR)
    c.part("belly", CREAM, line=False)
    c.part("ear", FUR)
    c.part("spade", DARK)
    c.part("head", FUR)
    c.part("face", CREAM, line=False)
    c.part("arm", FUR)
    c.ellipse("feet", 24, 59, 6, 3)
    c.ellipse("feet", 40, 59, 6, 3)
    c.ellipse("body", 32, 50, 11, 9)
    c.ellipse("belly", 32, 52, 7, 6)
    # long ears ending in huge shovel-shaped tips
    c.thick("ear", [(25, 26), (15, 15), (9, 7)], 6)
    c.thick("ear", [(39, 26), (49, 15), (55, 7)], 6)
    c.poly("spade", [(4, 14), (3, 2), (10, -1), (15, 5), (12, 14)])
    c.poly("spade", [(52, 14), (49, 5), (54, -1), (61, 2), (60, 14)])
    c.ellipse("head", 32, 32, 13, 11)
    c.ellipse("face", 32, 36, 9, 6)
    c.ellipse("arm", 23, 48, 3.5, 4)
    c.ellipse("arm", 41, 48, 3.5, 4)
    for ex in (25, 36):
        c.pxs([(ex, 29), (ex + 1, 29), (ex + 2, 29), (ex, 30), (ex + 1, 30), (ex + 2, 30), (ex + 1, 31)], EYE)
        c.px(ex, 29, WHITE)
    c.pxs([(31, 34), (32, 34), (33, 34), (32, 35)], (60, 40, 40))
    c.pxs([(31, 37), (31, 38), (33, 37), (33, 38)], WHITE)
    c.pxs([(32, 37), (32, 38), (30, 36), (34, 36)], (120, 90, 80))
    return c.render()


# ============================================================== XERNEAS (X)
def xerneas_front():
    c = Canvas(64, 64)
    BLUE = (66, 140, 214)
    NAVY = (34, 50, 90)
    c.part("leg", (40, 40, 52))
    c.part("body", BLUE)
    c.part("neck", BLUE)
    c.part("antler", NAVY, outline=(16, 20, 40))
    c.part("orbY", (255, 214, 60), line=False)
    c.part("orbP", (240, 110, 200), line=False)
    c.part("orbB", (110, 220, 255), line=False)
    c.part("orbO", (255, 150, 60), line=False)
    c.part("orbV", (160, 110, 240), line=False)
    c.part("head", BLUE)
    c.part("muzzle", (36, 40, 56))
    c.part("eye", (110, 230, 255), line=False, shade=False)
    # body + legs, standing proud
    c.thick("leg", [(40, 46), (38, 62)], 3.2)
    c.thick("leg", [(52, 46), (54, 62)], 3.2)
    c.thick("leg", [(30, 46), (26, 62)], 3.2)
    c.ellipse("body", 44, 44, 15, 8)
    c.poly("neck", [(22, 30), (32, 24), (40, 42), (28, 46)])
    # branching antler tree
    A = "antler"
    c.thick(A, [(18, 20), (12, 10), (6, 4)], 2.8)
    c.thick(A, [(12, 10), (3, 13)], 2.2)
    c.thick(A, [(14, 13), (10, 2)], 2.2)
    c.thick(A, [(28, 19), (36, 8), (44, 3)], 2.8)
    c.thick(A, [(36, 8), (46, 12)], 2.2)
    c.thick(A, [(32, 13), (32, 2)], 2.2)
    c.thick(A, [(40, 6), (50, 4)], 2.2)
    for name, (x, y, r) in (("orbY", (6, 4, 2.6)), ("orbP", (3, 13, 2.4)), ("orbB", (10, 2, 2.4)),
                            ("orbO", (44, 3, 2.6)), ("orbV", (46, 12, 2.6)), ("orbY", (32, 2, 2.4)),
                            ("orbB", (50, 4, 2.4)), ("orbP", (22, 9, 2))):
        c.circle(name, x, y, r + 1.1)
    c.thick(A, [(18, 14), (22, 9)], 1.3)
    # head, angled down-left
    c.ellipse("head", 23, 26, 9, 7)
    c.poly("head", [(16, 26), (10, 36), (16, 38), (26, 30)])
    c.poly("muzzle", [(10, 34), (9, 38), (15, 39), (17, 35)])
    c.poly("head", [(28, 22), (36, 18), (33, 25)])  # ear
    # star-cross X eye
    c.pxs([(18, 25), (20, 25), (19, 26), (18, 27), (20, 27)], (40, 90, 160))
    c.pxs([(19, 25), (18, 26), (20, 26), (19, 27)], (130, 240, 255))
    # chest glow lines
    c.pxs([(30, 40), (31, 41), (32, 42), (33, 43)], (150, 230, 255))
    return c.render()


# ============================================================== YVELTAL (Y)
def yveltal_front():
    c = Canvas(64, 64)
    DARK = (52, 50, 62)
    RED = (210, 40, 56)
    GREY = (110, 110, 124)
    c.part("wing", DARK, outline=(18, 16, 24))
    c.part("feather", RED, line=False)
    c.part("tail", DARK, outline=(18, 16, 24))
    c.part("tailred", RED, line=False)
    c.part("body", GREY)
    c.part("neck", DARK)
    c.part("head", DARK)
    c.part("crest", RED)
    # Y shaped silhouette: two great wings up and out, long tail below
    c.poly("wing", [(30, 30), (2, 2), (0, 12), (6, 16), (2, 22), (10, 26), (6, 32), (16, 34), (24, 40)])
    c.poly("wing", [(34, 30), (62, 2), (64, 12), (58, 16), (62, 22), (54, 26), (58, 32), (48, 34), (40, 40)])
    for pts in ([(4, 6), (24, 28), (20, 30), (2, 12)], [(4, 18), (22, 32), (18, 34), (5, 22)],
                [(60, 6), (40, 28), (44, 30), (62, 12)], [(60, 18), (42, 32), (46, 34), (59, 22)]):
        c.poly("feather", pts)
    c.poly("tail", [(26, 40), (38, 40), (36, 56), (32, 64), (28, 56)])
    c.poly("tailred", [(29, 54), (35, 54), (32, 62)])
    c.ellipse("body", 32, 37, 7, 8)
    c.thick("neck", [(32, 32), (30, 24), (26, 18)], 4)
    c.ellipse("head", 24, 15, 6, 5)
    c.poly("head", [(19, 15), (13, 19), (20, 19)])
    c.poly("crest", [(26, 11), (34, 4), (32, 12)])
    c.pxs([(21, 14), (22, 14), (22, 15)], (90, 230, 255))
    c.px(21, 15, WHITE)
    c.pxs([(30, 34), (32, 36), (34, 34), (32, 38)], RED)
    return c.render()


# ============================================================== ZYGARDE (Z)
def zygarde_front():
    c = Canvas(64, 64)
    BLACK_ = (40, 44, 48)
    GREEN = (110, 230, 96)
    c.part("legb", BLACK_, outline=(14, 16, 18))
    c.part("body", BLACK_, outline=(14, 16, 18))
    c.part("neck", BLACK_, outline=(14, 16, 18))
    c.part("fin", BLACK_, outline=(14, 16, 18))
    c.part("head", BLACK_, outline=(14, 16, 18))
    c.part("legf", BLACK_, outline=(14, 16, 18))
    c.part("hex", (70, 190, 90), line=False, shade=False)
    # long serpentine body with dog-like stance
    c.thick("legb", [(48, 44), (54, 54), (53, 62)], 5.5)
    c.ellipse("body", 42, 44, 16, 10)
    c.thick("neck", [(36, 42), (30, 30), (26, 20)], 9)
    # head crest fins (Z silhouette)
    c.poly("fin", [(28, 12), (44, 2), (36, 14)])
    c.poly("fin", [(32, 18), (50, 14), (36, 22)])
    c.ellipse("head", 24, 16, 9, 7)
    c.poly("head", [(18, 14), (8, 20), (18, 22)])
    c.thick("legf", [(32, 48), (26, 56), (24, 62)], 5.5)
    c.thick("legf", [(40, 50), (39, 62)], 5.5)
    # hexagon chest emblem
    c.poly("hex", [(31, 34), (35, 32), (39, 34), (39, 39), (35, 41), (31, 39)])
    zg = c
    G = GREEN
    # glowing green markings
    zg.pxs([(44, 4), (42, 5), (40, 6), (38, 7), (36, 8), (34, 9)], G)
    zg.pxs([(48, 15), (45, 16), (42, 17), (39, 18)], G)
    zg.pxs([(30, 26), (31, 28), (32, 30)], G)
    zg.pxs([(46, 40), (48, 41), (50, 42), (52, 43)], G)
    zg.pxs([(52, 52), (53, 54), (53, 56)], G)
    zg.pxs([(26, 56), (25, 58), (24, 60)], G)
    zg.pxs([(38, 54), (38, 56), (38, 58)], G)
    zg.pxs([(33, 35), (34, 34), (35, 34), (36, 34), (37, 35), (35, 36), (35, 37), (33, 38), (37, 38)], (60, 200, 80))
    # cyan hex eye
    zg.pxs([(20, 14), (21, 14), (20, 15), (21, 15), (22, 15)], (90, 255, 230))
    zg.px(20, 14, WHITE)
    return c.render()
ALL = {
    "pikachu_front": pikachu_front,
    "pikachu_back": pikachu_back,
    "fennekin_front": fennekin_front,
    "fennekin_back": fennekin_back,
    "froakie_front": froakie_front,
    "froakie_back": froakie_back,
    "chespin_front": chespin_front,
    "chespin_back": chespin_back,
    "fletchling_front": fletchling_front,
    "bunnelby_front": bunnelby_front,
    "xerneas_front": xerneas_front,
    "yveltal_front": yveltal_front,
    "zygarde_front": zygarde_front,
}

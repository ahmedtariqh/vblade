// SPDX-License-Identifier: CERN-OHL-P-2.0
// Source location: https://github.com/ahmedtariqh/vblade
// VBlade V0.1 — simulation hardware. Dimensions in mm.
part = "assembly"; // [assembly,blade_3,blade_4,handle,pod,pod_cover,display_stand]
preview_size = 3; // [3,4]
$fn = 32;

head_width = 29;
tongue_width = 23;
tongue_thickness = 9;
root_bolts = [-14,-4.7];
root_clearance = 4.4;
pod_bolts = [4,20];
pod_bolt_y = 17;
pod_clearance = 3.4;
pod_width = 26;
pod_length = 26;
pod_height = 28;
pod_pitch = -5;
pod_y = 10;
pod_board_cavity = [24,22.4,24];
lens_center = [-1.5,13.8];
lens_window = [12.6,14.4];
led_center = [8.5,13.8];
led_hole = 5.3;

function reach(size) = size == 3 ? 104 : 124;

// Capsules between section corners give the plate and web continuous round edges.
module section(x, y0, y1, z0, z1, r=1) {
    hull()
        for (y=[y0+r,y1-r], z=[z0+r,z1-r])
            translate([x,y,z]) sphere(r);
}

module loft(stations) {
    for (i=[0:len(stations)-2]) hull() {
        section(stations[i][0],stations[i][1],stations[i][2],
                stations[i][3],stations[i][4]);
        section(stations[i+1][0],stations[i+1][1],stations[i+1][2],
                stations[i+1][3],stations[i+1][4]);
    }
}

module rounded_rectangle(w,h,r) {
    hull()
        for (x=[-w/2+r,w/2-r], y=[-h/2+r,h/2-r])
            translate([x,y]) circle(r);
}

// Extrude in +X from a profile drawn in the YZ plane.
module yz_extrusion(x,y,z,depth) {
    translate([x,y,z])
        multmatrix([[0,0,1,0],[1,0,0,0],[0,1,0,0],[0,0,0,1]])
            linear_extrude(height=depth) children();
}

module pod_pose(size) {
    translate([reach(size)-46,pod_y,size == 3 ? 14.4 : 15.4])
        rotate([0,pod_pitch,0]) children();
}

module pod_seat(size) {
    // Each flat landing pad is continuous with the curved side web.
    for (x=pod_bolts)
        pod_pose(size) hull() {
            translate([x,pod_bolt_y,-5.5]) cylinder(h=5.5,r=5.5);
            translate([x,-0.7,-5]) cylinder(h=5,r=5);
        }
}

module blade(size) {
    l = reach(size);
    delta = size == 4 ? 1 : 0;
    difference() {
        union() {
            translate([-30,-tongue_width/2,0])
                cube([35,tongue_width,tongue_thickness]);
            loft([
                [1,-13,13,0,7], [18,-15-delta,15,0.7,5.9],
                [l*0.38,-14-delta,14,2.4,7.4],
                [l*0.56,-13-delta,13,5,9.6],
                [l*0.73,-11-delta,12,8.4,12.8],
                [l*0.88,-9-delta,10,12.3,16.3],
                [l-4,-7,8,16.5+delta,20+delta],
                [l-1,-4.3,4.8,17.5+delta,20+delta]
            ]);
            loft([
                [1,7,13,1,20], [16,9,15,2.5,24],
                [l*0.35,8,14,4,23], [l*0.54,7,13,6,20],
                [l*0.71,6,12,8,17.2], [l*0.86,5,10,11.7,16.2],
                [l-6,4,8,15.8+delta,19+delta]
            ]);
            pod_seat(size);
        }
        for (x=root_bolts)
            translate([x,0,-1]) cylinder(h=12,d=root_clearance);
        pod_pose(size)
            translate([-3.1,-13.3,-0.12]) cube([29.6,26.6,45]);
        pod_pose(size)
            for (x=pod_bolts)
                translate([x,pod_bolt_y,-14]) cylinder(h=23,d=pod_clearance);
        for (x=[14,l*0.39])
            translate([x,15,13])
                rotate([90,0,0]) linear_extrude(height=10)
                    rounded_rectangle(4.8,3.6,1.1);
    }
}

module handle() {
    difference() {
        union() {
            hull() {
                translate([-33,0,-103]) cylinder(h=3,r=13.5);
                translate([-34,0,-32]) cylinder(h=3,r=14.5);
                translate([-35,0,-10]) cylinder(h=3,r=13.5);
            }
            translate([-44,-head_width/2,-9]) cube([44,head_width,9]);
            // Narrower behind the bolts, so washers and a wrench stay accessible.
            hull() {
                translate([-36,0,-30]) cylinder(h=4,r=10.5);
                translate([-32,0,-10]) cylinder(h=3,r=11);
            }
            for (y=[-head_width/2,tongue_width/2+0.35])
                translate([-30,y,-0.8])
                    cube([29.8,(head_width-tongue_width)/2-0.35,3.4]);
        }
        for (x=root_bolts)
            translate([x,0,-18]) cylinder(h=22,d=root_clearance);
        translate([-34,-13.15,-65]) rotate([90,0,0]) rotate([0,0,90])
            linear_extrude(height=2.3)
                text("SIM ONLY",size=4,halign="center",valign="center");
        mirror([0,1,0]) translate([-34,-13.15,-65]) rotate([90,0,0]) rotate([0,0,90])
            linear_extrude(height=2.3)
                text("SIM ONLY",size=4,halign="center",valign="center");
    }
}

cover_bolts = [[-10.9,3.8],[10.9,24.2]];

module pod() {
    difference() {
        union() {
            yz_extrusion(0,0,pod_height/2,pod_length)
                rounded_rectangle(pod_width,pod_height,2.7);
            for (x=pod_bolts)
                hull() {
                    translate([x,11,0]) cylinder(h=3.5,r=3.9);
                    translate([x,pod_bolt_y,0]) cylinder(h=3.5,r=4.5);
                }
            for (p=cover_bolts)
                yz_extrusion(0,p[0],p[1],5.8) circle(r=3.0);
        }
        difference() {
            yz_extrusion(-1,0,pod_height/2,pod_board_cavity[0]+1)
                rounded_rectangle(pod_board_cavity[1],pod_board_cavity[2],1.1);
            for (p=cover_bolts)
                yz_extrusion(-1,p[0],p[1],6.8) circle(r=3.0);
        }
        yz_extrusion(pod_length-3.1,lens_center[0],lens_center[1],4.3)
            rounded_rectangle(lens_window[0],lens_window[1],1.4);
        yz_extrusion(pod_length-3.1,led_center[0],led_center[1],4.3)
            circle(d=led_hole);
        for (x=pod_bolts)
            translate([x,pod_bolt_y,-1]) cylinder(h=5.7,d=pod_clearance);
        translate([-1,-23,-1]) cube([1,46,30]);
        // M2.5 screw pilots for the rear cover.
        for (p=cover_bolts)
            yz_extrusion(-0.5,p[0],p[1],5.7) circle(d=2.1);
    }
}

module pod_cover() {
    difference() {
        yz_extrusion(-2.6,0,pod_height/2,2.6)
            rounded_rectangle(pod_width,pod_height,2.7);
        for (p=cover_bolts)
            yz_extrusion(-3,p[0],p[1],3.5) circle(d=2.8);
        yz_extrusion(-3,0,10,3.5) rounded_rectangle(12.2,8.5,1.4);
        // Two strain-relief tie passages in the cover, separate from the aperture.
        for (z=[3.6,17])
            yz_extrusion(-3,0,z,3.5) rounded_rectangle(5.1,2.6,0.65);
    }
}

module display_stand() {
    // PCB is retained on these arms using insulated hook-and-loop strips.
    union() {
        linear_extrude(height=4) rounded_rectangle(69,54,4);
        for (x=[-23,23]) translate([x,0,3.8])
            rotate([90,0,90]) linear_extrude(height=11,center=true)
                polygon([[-18,0],[19,0],[9,35],[-18,1.5]]);
        translate([0,-17,2])
            linear_extrude(height=3) rounded_rectangle(61,7,2);
    }
}

module assembly(size) {
    color("#24364b") handle();
    color(size == 3 ? "#34918a" : "#5272b3") blade(size);
    pod_pose(size) {
        color("#da793e") pod();
        color("#b3542d") pod_cover();
    }
    translate([15,-53,0]) color("#24364b") display_stand();
}

if (part == "assembly") assembly(preview_size);
if (part == "blade_3") blade(3);
if (part == "blade_4") blade(4);
if (part == "handle") translate([0,0,103]) handle();
if (part == "pod") translate([0,0,pod_length]) rotate([0,90,0]) pod();
if (part == "pod_cover") rotate([0,90,0]) pod_cover();
if (part == "display_stand") display_stand();

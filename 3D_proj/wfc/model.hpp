#ifndef WFC_UTILS_MODEL_HPP_
#define WFC_UTILS_MODEL_HPP_

#include <vector>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>

using namespace std;

struct POINT3 {
	double X;
	double Y;
	double Z;
};
struct Texture {
	double TU;
	double TV;
};
struct normals {
	double NX;
	double NY;
	double NZ;
};
struct Face {
	int V[3];
	int T[3];
	int N[3];
};

class ObjModel {
public:
	vector<POINT3> V;//V：代表顶点。格式为V X Y Z，V后面的X Y Z表示三个顶点坐标。浮点型
	vector<Texture>  VT;//表示纹理坐标。格式为VT TU TV。浮点型
	vector<normals> VN;//VN：法向量。每个三角形的三个顶点都要指定一个法向量。格式为VN NX NY NZ。浮点型
	vector<Face> F;//F：面。面后面跟着的整型值分别是属于这个面的顶点、纹理坐标、法向量的索引。
				   //面的格式为：f Vertex1/Texture1/Normal1 Vertex2/Texture2/Normal2 Vertex3/Texture3/Normal3
};


optional<ObjModel> ReadModel(const string file_path)
{
	ifstream ifs(file_path);//cube bunny Eight
	string s;
	Face *f;
	POINT3 *v;
	normals *vn;
	Texture    *vt;
	ObjModel my_model;
	while (getline(ifs, s))
	{
		if (s.length()<2)continue;
		if (s[0] == 'v') {
			if (s[1] == 't') {//vt 纹理
				istringstream in(s);
				vt = new Texture();
				string head;
				in >> head >> vt->TU >> vt->TV;
				my_model.VT.push_back(*vt);
			}
			else if (s[1] == 'n') {//vn  法向量
				istringstream in(s);
				vn = new normals();
				string head;
				in >> head >> vn->NX >> vn->NY >> vn->NZ;
				my_model.VN.push_back(*vn);
			}
			else {//v  点
				istringstream in(s);
				v = new POINT3();
				string head;
				in >> head >> v->X >> v->Y >> v->Z;
				my_model.V.push_back(*v);
			}
		}
		else if (s[0] == 'f') {//f  面
			for (int k = s.size() - 1; k >= 0; k--) {
				if (s[k] == '/')s[k] = ' ';
			}
			istringstream in(s);
			f = new Face();
			string head;
			in >> head;
			int i = 0;
			while (i<3)
			{
				if (my_model.V.size() != 0)
				{
					in >> f->V[i];
					f->V[i] -= 1;
				}
				if (my_model.VT.size() != 0)
				{
					in >> f->T[i];
					f->T[i] -= 1;
				}
				if (my_model.VN.size() != 0)
				{
					in >> f->N[i];
					f->N[i] -= 1;
				}
				i++;
			}
			my_model.F.push_back(*f);
		}
	}
	return my_model;
}




void WriteModel(const string aim_path, ObjModel &temp) {
	ofstream myfile;
	myfile.open(aim_path);
	for (unsigned i = 0; i < temp.V.size(); i++) {
		myfile << "v " << setprecision(8) << temp.V[i].X << " " << temp.V[i].Y << " " << temp.V[i].Z << "\n";
	}
	myfile << "\n";
	//for (unsigned i = 0; i < temp.VT.size(); i++){
	//	myfile << "vt " << setprecision(8) << temp.VT[i].TU << " " << temp.VT[i].TV << "\n";
	//}
	//myfile << "\n";
	for (unsigned i = 0; i < temp.VN.size(); i++) {
		myfile << "vn " << setprecision(8) << temp.VN[i].NX << " " << temp.VN[i].NY << " " << temp.VN[i].NZ << "\n";
	}
	myfile << "\n";

	for (unsigned i = 0; i < temp.F.size(); i++) {
		myfile << "f ";
		for (unsigned j = 0; j < 3; j++) {
			myfile << temp.F[i].V[j] + 1 << "/" << temp.F[i].T[j] + 1 << "/" << temp.F[i].N[j] + 1;
			if (j != 2) {
				myfile << " ";
			}
		}
		myfile << "\n";
	}
	myfile.close();

}


ObjModel RotatedModel(ObjModel &temp) {
	for (unsigned i = 0; i < temp.V.size(); i++) {
		auto m = temp.V[i].X;
		temp.V[i].X = temp.V[i].Z;
		temp.V[i].Z = -1 * m;
	}
	return temp;
}



#endif // !WFC_UTILS_MODEL_HPP_


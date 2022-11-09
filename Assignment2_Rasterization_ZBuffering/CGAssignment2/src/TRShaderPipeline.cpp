#include "TRShaderPipeline.h"

#include <algorithm>
#include <thread>
namespace TinyRenderer
{
	//----------------------------------------------VertexData----------------------------------------------

	TRShaderPipeline::VertexData TRShaderPipeline::VertexData::lerp(
		const TRShaderPipeline::VertexData& v0,
		const TRShaderPipeline::VertexData& v1,
		float frac)
	{
		//Linear interpolation
		VertexData result;
		result.pos = (1.0f - frac) * v0.pos + frac * v1.pos;
		result.col = (1.0f - frac) * v0.col + frac * v1.col;
		result.nor = (1.0f - frac) * v0.nor + frac * v1.nor;
		result.tex = (1.0f - frac) * v0.tex + frac * v1.tex;
		result.cpos = (1.0f - frac) * v0.cpos + frac * v1.cpos;
		result.spos.x = (1.0f - frac) * v0.spos.x + frac * v1.spos.x;
		result.spos.y = (1.0f - frac) * v0.spos.y + frac * v1.spos.y;

		return result;
	}

	TRShaderPipeline::VertexData TRShaderPipeline::VertexData::barycentricLerp(
		const VertexData& v0,
		const VertexData& v1,
		const VertexData& v2,
		glm::vec3 w)
	{
		VertexData result;
		result.pos = w.x * v0.pos + w.y * v1.pos + w.z * v2.pos;
		result.col = w.x * v0.col + w.y * v1.col + w.z * v2.col;
		result.nor = w.x * v0.nor + w.y * v1.nor + w.z * v2.nor;
		result.tex = w.x * v0.tex + w.y * v1.tex + w.z * v2.tex;
		result.cpos = w.x * v0.cpos + w.y * v1.cpos + w.z * v2.cpos;
		result.spos.x = w.x * v0.spos.x + w.y * v1.spos.x + w.z * v2.spos.x;
		result.spos.y = w.x * v0.spos.y + w.y * v1.spos.y + w.z * v2.spos.y;

		return result;
	}

	void TRShaderPipeline::VertexData::prePerspCorrection(VertexData& v)
	{
		//Perspective correction: the world space properties should be multipy by 1/w before rasterization
		//https://zhuanlan.zhihu.com/p/144331875
		//We use pos.w to store 1/w
		v.pos.w = 1.0f / v.cpos.w;
		v.pos = glm::vec4(v.pos.x * v.pos.w, v.pos.y * v.pos.w, v.pos.z * v.pos.w, v.pos.w);
		v.tex = v.tex * v.pos.w;
		v.nor = v.nor * v.pos.w;
		v.col = v.col * v.pos.w;
	}

	void TRShaderPipeline::VertexData::aftPrespCorrection(VertexData& v)
	{
		//Perspective correction: the world space properties should be multipy by w after rasterization
		//https://zhuanlan.zhihu.com/p/144331875
		//We use pos.w to store 1/w
		float w = 1.0f / v.pos.w;
		v.pos = v.pos * w;
		v.tex = v.tex * w;
		v.nor = v.nor * w;
		v.col = v.col * w;
	}

	//----------------------------------------------TRShaderPipeline----------------------------------------------

	void TRShaderPipeline::rasterize_wire(
		const VertexData& v0,
		const VertexData& v1,
		const VertexData& v2,
		const unsigned int& screen_width,
		const unsigned int& screene_height,
		std::vector<VertexData>& rasterized_points)
	{
		//Draw each line step by step
		rasterize_wire_aux(v0, v1, screen_width, screene_height, rasterized_points);
		rasterize_wire_aux(v1, v2, screen_width, screene_height, rasterized_points);
		rasterize_wire_aux(v0, v2, screen_width, screene_height, rasterized_points);

	}
	bool TRShaderPipeline::InsideTriangle(const glm::ivec2& v0, const glm::ivec2& v1, const glm::ivec2& v2, const glm::vec2& p) {
		glm::vec3 e0 = glm::vec3(v1 - v0, 1.0f);
		glm::vec3 e1 = glm::vec3(v2 - v1, 1.0f);
		glm::vec3 e2 = glm::vec3(v0 - v2, 1.0f);
		glm::vec3 point = glm::vec3(p, 1.0f);
		bool f0 = (glm::cross((point - glm::vec3(v0, 1.0f)), e0).z >= 0);
		bool f1 = glm::cross((point - glm::vec3(v1, 1.0f)), e1).z >= 0;
		bool f2 = glm::cross((point - glm::vec3(v2, 1.0f)), e2).z >= 0;
		return (f0 == f1) && (f1 == f2);
	}

	void TRShaderPipeline::rasterize_fill_edge_function(
		const VertexData& v0,
		const VertexData& v1,
		const VertexData& v2,
		const unsigned int& screen_width,
		const unsigned int& screene_height,
		std::vector<VertexData>& rasterized_points)
	{
		int minX, maxX, minY, maxY;
		minX = maxX = v0.spos.x;
		minY = maxY = v0.spos.y;

		minX = std::min(minX, v1.spos.x);
		minX = std::min(minX, v2.spos.x);
		maxX = std::max(maxX, v1.spos.x);
		maxX = std::max(maxX, v2.spos.x);
		minY = std::min(minY, v1.spos.y);
		minY = std::min(minY, v2.spos.y);
		maxY = std::max(maxY, v1.spos.y);
		maxY = std::max(maxY, v2.spos.y);
		for (int i = minX; i <= maxX; i++) {
			for (int j = minY; j <= maxY; j++) {
				glm::vec3 color = { 0.0f , 0.0f , 0.0f };
				glm::vec2 tmp0 = { i - 0.25 , j + 0.25 };
				glm::vec2 tmp1 = { i - 0.25 , j - 0.25 };
				glm::vec2 tmp2 = { i + 0.25 , j + 0.25 };
				glm::vec2 tmp3 = { i + 0.25 , j - 0.25 };
				std::vector<glm::vec2> tmp_points = { tmp0 , tmp1 , tmp2 , tmp3 };
				int num = 0;
				for (auto tmp_p : tmp_points) {
					if (InsideTriangle(v0.spos, v1.spos, v2.spos, tmp_p)) {
						num += 1;
					}
				}
				if (num > 0) {
					glm::vec2 pot = glm::vec2(i, j);
					glm::vec2 l1 = v1.spos - v0.spos;
					glm::vec2 l2 = v2.spos - v0.spos;
					glm::vec2 l3 = glm::vec2(v0.spos) - pot;
					glm::vec3 xvector = glm::vec3(l1.x, l2.x, l3.x);
					glm::vec3 yvector = glm::vec3(l1.y, l2.y, l3.y);
					glm::vec3 u = glm::cross(xvector, yvector);
					glm::vec3 w = glm::vec3(1.0f - (u.x + u.y) / u.z, u.x / u.z, u.y / u.z);
					VertexData tmp = VertexData::barycentricLerp(v0, v1, v2, w);
					tmp.spos.x = i;
					tmp.spos.y = j;
					tmp.col = glm::vec3{ tmp.col.x / num , tmp.col.y / num , tmp.col.z / num };
					rasterized_points.push_back(tmp);
				}
			}
		}
		//Edge-function rasterization algorithm

		//Task4: Implement edge-function triangle rassterization algorithm
		// Note: You should use VertexData::barycentricLerp(v0, v1, v2, w) for interpolation, 
		//       interpolated points should be pushed back to rasterized_points.
		//       Interpolated points shold be discarded if they are outside the window. 

		//       v0.spos, v1.spos and v2.spos are the screen space vertices.

		//For instance:

	}

	void TRShaderPipeline::rasterize_wire_aux(
		const VertexData& from,
		const VertexData& to,
		const unsigned int& screen_width,
		const unsigned int& screen_height,
		std::vector<VertexData>& rasterized_points)
	{
		int dx = to.spos.x - from.spos.x;
		int dy = to.spos.y - from.spos.y;
		int stepX = 1, stepY = 1;

		// 判断from点和to点的相对位置
		if (dx < 0)
		{
			stepX = -1;
			dx = -dx;
		}
		if (dy < 0)
		{
			stepY = -1;
			dy = -dy;
		}

		// 计算d2y - d2x
		int d2x = 2 * dx, d2y = 2 * dy;
		int d2y_minus_d2x = d2y - d2x;
		int sx = from.spos.x;
		int sy = from.spos.y;

		// slope < 1.
		if (dy <= dx)
		{
			// p0
			int p = d2y - dx;
			for (int i = 0; i <= dx; ++i)
			{
				// linear interpolation
				VertexData tmp = VertexData::lerp(from, to, static_cast<double>(i) / dx);
				sx += stepX;
				if (p <= 0)
					p += d2y;
				else
				{
					sy += stepY;
					p += d2y_minus_d2x;
				}
				tmp.spos.x = sx;
				tmp.spos.y = sy;
				if (tmp.spos.x < screen_width && tmp.spos.y < screen_height) {
					rasterized_points.push_back(tmp);
				}
			}
		}

		// slope > 1.
		else
		{
			int p = d2x - dy;
			for (int i = 0; i <= dy; ++i)
			{
				// linear interpolation
				VertexData tmp = VertexData::lerp(from, to, static_cast<double>(i) / dy);
				sy += stepY;
				if (p <= 0)
					p += d2x;
				else
				{
					sx += stepX;
					p -= d2y_minus_d2x;
				}
				tmp.spos.x = sx;
				tmp.spos.y = sy;
				if (tmp.spos.x < screen_width && tmp.spos.y < screen_height) {
					rasterized_points.push_back(tmp);
				}
			}
		}



		//Task1: Implement Bresenham line rasterization
		// Note: You shold use VertexData::lerp(from, to, weight) for interpolation,
		//       interpolated points should be pushed back to rasterized_points.
		//       Interpolated points shold be discarded if they are outside the window. 

		//       from.spos and to.spos are the screen space vertices.

		//For instance:
		rasterized_points.push_back(from);
		rasterized_points.push_back(to);

	}

	void TRDefaultShaderPipeline::vertexShader(VertexData& vertex)
	{
		//Local space -> World space -> Camera space -> Project space
		vertex.pos = m_model_matrix * glm::vec4(vertex.pos.x, vertex.pos.y, vertex.pos.z, 1.0f);
		vertex.cpos = m_view_project_matrix * vertex.pos;
	}

	void TRDefaultShaderPipeline::fragmentShader(const VertexData& data, glm::vec4& fragColor)
	{
		//Just return the color.
		fragColor = glm::vec4(data.tex, 0.0, 1.0f);
	}
}
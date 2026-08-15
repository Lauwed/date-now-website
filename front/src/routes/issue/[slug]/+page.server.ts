import { PUBLIC_API_URL } from '$env/static/public';
import type { Article, Response } from '../../../types';
import type { PageServerLoad } from './$types';

export const load: PageServerLoad = async ({ params }): Promise<Response<Article>> => {
	const res = await fetch(`${PUBLIC_API_URL}/api/issue/${params.slug}`, {
		method: 'GET'
	});

	if (!res.ok) {
		return {
			success: false,
			data: null,
			message: 'Something went wrong',
			status: res.status
		};
	}

	const data = await res.json();

	return { success: true, data, status: res.status };
};
